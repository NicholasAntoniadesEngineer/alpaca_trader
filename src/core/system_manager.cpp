#include "core/system_manager.hpp"
#include "core/system_state.hpp"
#include "core/trading_system_modules.hpp"
#include "core/system_threads.hpp"
#include "threads/thread_logic/thread_registry.hpp"
#include "threads/thread_logic/thread_manager.hpp"
#include "logging/startup_logs.hpp"
#include "logging/async_logger.hpp"
#include "logging/account_logs.hpp"
#include "configs/system_config.hpp"
#include "configs/component_configs.hpp"
#include "core/trader.hpp"
#include "core/account_manager.hpp"
#include "api/alpaca_client.hpp"
#include "threads/system_threads/logging_thread.hpp"
#include "threads/system_threads/market_data_thread.hpp"
#include "threads/system_threads/account_data_thread.hpp"
#include "threads/system_threads/market_gate_thread.hpp"
#include "threads/system_threads/trader_thread.hpp"
#include "core/data_structures.hpp"
#include <iostream>
#include <csignal>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

using namespace AlpacaTrader::Core;
using namespace AlpacaTrader::Logging;
using namespace AlpacaTrader::Threads;
using namespace ThreadSystem;

// Global signal handling
static SystemState* g_system_state = nullptr;

void signal_handler(int signal) {
    (void)signal; // Suppress unused parameter warning
    if (g_system_state) {
        g_system_state->running.store(false);
        g_system_state->cv.notify_all();
    }
}

void setup_signal_handlers(SystemState& system_state) {
    g_system_state = &system_state;
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
}


void log_startup_information(const TradingSystemModules& modules, const AlpacaTrader::Config::SystemConfig& config) {
    // Log application header
    StartupLogs::log_application_header();
    
    // Note: Main startup logging is handled separately to avoid duplication
    
    // Log account information if available
    if (modules.portfolio_manager) {
        StartupLogs::log_account_overview(*modules.portfolio_manager);
        StartupLogs::log_financial_summary(*modules.portfolio_manager);
        StartupLogs::log_current_positions(*modules.portfolio_manager);
    }
    
    // Log data source configuration
    StartupLogs::log_data_source_configuration(config);
    
    // Log runtime configuration
    StartupLogs::log_runtime_configuration(config);
    
    // Log strategy configuration
    StartupLogs::log_strategy_configuration(config);
    
    // Log thread system startup
    StartupLogs::log_thread_system_startup(config.timing);
}

void configure_trading_modules(SystemThreads& handles, TradingSystemModules& modules) {
    // Configure modules with iteration counters
    if (modules.trading_engine) {
        modules.trading_engine->set_iteration_counter(handles.trader_iterations);
    }
}

SystemThreads SystemManager::startup(SystemState& system_state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger) {
    // Create handles for the threads
    SystemThreads handles;
    
    // Initialize the global logging system FIRST
    if (logger) {
        AlpacaTrader::Logging::initialize_global_logger(*logger);
    }
    
    // Create all trading system modules and store them in system_state for lifetime management
    system_state.trading_modules = std::make_unique<TradingSystemModules>(create_trading_modules(system_state, logger));
    
    // Attach shared state to trader
    system_state.trading_modules->trading_engine->attach_shared_state(system_state.mtx, system_state.cv, system_state.market, system_state.account, system_state.has_market, system_state.has_account, system_state.running, system_state.allow_fetch);
    
    // Log startup information
    log_startup_information(*system_state.trading_modules, system_state.config);
    
    // Configure trading modules
    configure_trading_modules(handles, *system_state.trading_modules);
    
    // Setup signal handling for graceful shutdown
    setup_signal_handlers(system_state);
    
    // Create thread configurations from single source
    auto& modules = *system_state.trading_modules;
    auto thread_definitions = AlpacaTrader::Core::ThreadRegistry::create_thread_definitions(handles, modules);
    auto thread_infos = AlpacaTrader::Core::ThreadRegistry::create_thread_infos(thread_definitions);
    
    // Start all threads
    try {
        Manager::start_threads(thread_definitions);
    } catch (const std::exception& e) {
        std::cerr << "Error starting threads: " << e.what() << std::endl;
        return handles;
    }
    
    // Setup thread priorities after threads are started
    try {
        Manager::setup_thread_priorities(thread_definitions, system_state.config);
    } catch (const std::exception& e) {
        std::cerr << "Error setting thread priorities: " << e.what() << std::endl;
        return handles;
    }
    
    // Note: Main startup logging moved to run_until_shutdown to avoid conflicts with threads
    
    // Store thread infos for monitoring
    system_state.thread_infos = std::move(thread_infos);
    
    return handles;
}

static void run_until_shutdown(SystemState& state, SystemThreads& /*handles*/) {
    // Simple main loop without thread monitoring to prevent crashes
    try {
        // Ensure running flag is properly initialized
        if (!state.running.load()) {
            std::cerr << "Warning: running flag is false at start" << std::endl;
            state.running.store(true);
        }
        
        while (state.running.load()) {
            try {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } catch (const std::exception& e) {
                // Log error and continue
                std::cerr << "Error in main loop: " << e.what() << std::endl;
            } catch (...) {
                // Log unknown error and continue
                std::cerr << "Unknown error in main loop" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal error in run_until_shutdown: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown fatal error in run_until_shutdown" << std::endl;
    }
}

void SystemManager::shutdown(SystemState& system_state, SystemThreads& /*handles*/, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger) {
    // Signal all threads to stop
    system_state.cv.notify_all();
    
    // Wait for all threads to complete
    Manager::shutdown_threads();
    
    // Shutdown logging system
    AlpacaTrader::Logging::shutdown_global_logger(*logger);
}

// Thread management functions moved to thread_registry.hpp/.cpp
// This provides a single source of truth for all thread definitions

void SystemManager::run(SystemState& system_state, SystemThreads& handles) {
    run_until_shutdown(system_state, handles);
}