#include "system_manager.hpp"
#include "system_state.hpp"
#include "system_modules.hpp"
#include "system_threads.hpp"
#include "trading_system_factory.hpp"
#include "core/threads/thread_logic/thread_registry.hpp"
#include "core/threads/thread_logic/thread_manager.hpp"
#include "core/logging/startup_logs.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/account_logs.hpp"
#include "configs/system_config.hpp"
#include "core/system/system_configurations.hpp"
#include "core/trader/trader.hpp"
#include "core/trader/data/account_manager.hpp"
#include "core/trader/data/market_data_fetcher.hpp"
#include "api/general/api_manager.hpp"
#include "core/threads/system_threads/logging_thread.hpp"
#include "core/threads/system_threads/market_data_thread.hpp"
#include "core/threads/system_threads/account_data_thread.hpp"
#include "core/threads/system_threads/market_gate_thread.hpp"
#include "core/threads/system_threads/trader_thread.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/threads/thread_register.hpp"
#include <iostream>
#include <csignal>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

using namespace AlpacaTrader::Core;
using namespace AlpacaTrader::Logging;
using namespace AlpacaTrader::Threads;
using namespace AlpacaTrader::Core::ThreadSystem;

// Global signal handling
static SystemState* g_system_state = nullptr;

void signal_handler(int /* signal */) {
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


void log_startup_information(const SystemModules& modules, const AlpacaTrader::Config::SystemConfig& config) {
    // Log application header
    StartupLogs::log_application_header();
    
    // Log API endpoints table
    StartupLogs::log_api_endpoints_table(config);
    
    // Note: Main startup logging is handled separately to avoid duplication
    
    // Log account information if available
    if (modules.portfolio_manager) {
        StartupLogs::log_account_overview(*modules.portfolio_manager);
        StartupLogs::log_financial_summary(*modules.portfolio_manager);
        StartupLogs::log_current_positions(*modules.portfolio_manager, config);
    }
    
    // Log data source configuration
    StartupLogs::log_data_source_configuration(config);
    
    // Log runtime configuration
    StartupLogs::log_runtime_configuration(config);
    
    // Log strategy configuration
    StartupLogs::log_strategy_configuration(config);
    
    // Log thread system startup
    StartupLogs::log_thread_system_startup(config);
}

SystemConfigurations create_trading_configurations(const SystemState& state) {
    return SystemConfigurations{
        AccountManagerConfig{state.config.logging, state.config.timing, state.config.strategy},
        MarketDataThreadConfig{state.config.strategy, state.config.timing},
        AccountDataThreadConfig{state.config.timing}
    };
}

SystemModules create_trading_modules(SystemState& state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger) {
    SystemConfigurations configs = create_trading_configurations(state);
    SystemModules modules;

    // Create core trading modules using the factory
    auto trading_components = TradingSystemFactory::create_trading_system(state.config);
    
    modules.api_manager = std::move(trading_components.api_manager);
    modules.portfolio_manager = std::move(trading_components.account_manager);
    modules.trading_engine = std::move(trading_components.trading_orchestrator);
    modules.account_dashboard = std::make_unique<AlpacaTrader::Logging::AccountLogs>(state.config.logging, *modules.portfolio_manager);
    
    // Create thread modules
    
    // Create MARKET_DATA thread
    modules.market_data_thread = std::make_unique<AlpacaTrader::Threads::MarketDataThread>(configs.market_data_thread, *modules.api_manager,
                                                                   state.mtx, state.cv, state.market,
                                                                   state.has_market, state.running,
                                                                   state.market_data_timestamp, state.market_data_fresh);
    
    // Create ACCOUNT_DATA thread
    modules.account_data_thread = std::make_unique<AlpacaTrader::Threads::AccountDataThread>(configs.account_data_thread, *modules.portfolio_manager, 
                                                                     state.mtx, state.cv, state.account, 
                                                                     state.has_account, state.running);
    
    // Create MARKET_GATE thread
    modules.market_gate_thread = std::make_unique<AlpacaTrader::Threads::MarketGateThread>(state.config.timing, state.config.logging,
                                                                   state.allow_fetch, state.running, *modules.api_manager);
    
    // Create LOGGING thread
    static std::atomic<unsigned long> logging_iterations{0};
    modules.logging_thread = std::make_unique<AlpacaTrader::Threads::LoggingThread>(logger, logging_iterations, state.config);
    
    // Create TRADER_DECISION thread
    static std::atomic<unsigned long> trader_iterations{0};
    modules.trading_thread = std::make_unique<AlpacaTrader::Threads::TraderThread>(*modules.trading_engine, trader_iterations, state.config.timing);
    
    return modules;
}

void configure_trading_modules(SystemThreads& handles, SystemModules& modules, SystemState& state) {
    // Configure thread iteration counters using the generic registry approach
    AlpacaTrader::Core::ThreadRegistry::configure_thread_iteration_counters(handles, modules);
    
    // Configure allow_fetch_ptr for threads that need it
    if (modules.market_data_thread) {
        modules.market_data_thread->set_allow_fetch_flag(state.allow_fetch);
    }
    if (modules.account_data_thread) {
        modules.account_data_thread->set_allow_fetch_flag(state.allow_fetch);
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
    system_state.trading_modules = std::make_unique<SystemModules>(create_trading_modules(system_state, logger));
    
        // Set up data synchronization for trading orchestrator
        DataSyncConfig sync_config(system_state.mtx, system_state.cv, system_state.market, system_state.account, 
                                   system_state.has_market, system_state.has_account, system_state.running, system_state.allow_fetch,
                                   system_state.market_data_timestamp, system_state.market_data_fresh, system_state.last_order_timestamp);
        system_state.trading_modules->trading_engine->setup_data_synchronization(sync_config);
    
    // Log startup information
    log_startup_information(*system_state.trading_modules, system_state.config);
    
    // Configure trading modules
    configure_trading_modules(handles, *system_state.trading_modules, system_state);
    
    // Setup signal handling for graceful shutdown
    setup_signal_handlers(system_state);
    
    // Create thread configurations from single source
    auto& modules = *system_state.trading_modules;
    auto thread_definitions = AlpacaTrader::Core::ThreadRegistry::create_thread_definitions(handles, modules);
    auto thread_infos = AlpacaTrader::Core::ThreadRegistry::create_thread_infos(thread_definitions);
    
    // Start all threads
    try {
        Manager::start_threads(thread_definitions, modules);
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

static void run_until_shutdown(SystemState& state) {
    try {
        // Ensure running flag is properly initialized
        if (!state.running.load()) {
            std::cerr << "Warning: running flag is false at start" << std::endl;
            state.running.store(true);
        }
        
        auto start_time = std::chrono::steady_clock::now();
        auto last_monitor_time = start_time;
        
        while (state.running.load()) {
            try {
                auto now = std::chrono::steady_clock::now();

                // Check if thread monitoring is enabled and it's time to log stats (configurable frequency)
                if (state.config.timing.enable_system_health_monitoring &&
                    !state.thread_infos.empty() &&
                    std::chrono::duration_cast<std::chrono::seconds>(now - last_monitor_time).count() >= state.config.timing.system_health_logging_interval_seconds) {

                    try {
                        AlpacaTrader::Core::ThreadSystem::Manager::log_thread_monitoring_stats(state.thread_infos, start_time);
                        last_monitor_time = now;
                    } catch (const std::exception& e) {
                        std::cerr << "Error logging thread monitoring stats: " << e.what() << std::endl;
                    } catch (...) {
                        std::cerr << "Unknown error logging thread monitoring stats" << std::endl;
                    }
                }

                // Sleep for main loop interval based on configuration
                std::this_thread::sleep_for(std::chrono::seconds(state.config.timing.thread_market_data_poll_interval_sec));
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

void SystemManager::shutdown(SystemState& system_state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger) {
    // Signal all threads to stop
    system_state.cv.notify_all();

    // Wait for all threads to complete
    Manager::shutdown_threads();

    // Cleanup API manager - handled automatically by unique_ptr
    if (system_state.trading_modules->api_manager) {
        system_state.trading_modules->api_manager->shutdown();
    }

    // Shutdown logging system
    AlpacaTrader::Logging::shutdown_global_logger(*logger);
}

// Thread management functions moved to thread_registry.hpp/.cpp
// This provides a single source of truth for all thread definitions

void SystemManager::run(SystemState& system_state) {
    run_until_shutdown(system_state);
}