#include "core/system_manager.hpp"
#include "core/system_state.hpp"
#include "core/trading_system_modules.hpp"
#include "core/system_threads.hpp"
#include "threads/thread_manager.hpp"
#include "logging/startup_logs.hpp"
#include "logging/async_logger.hpp"
#include "configs/system_config.hpp"
#include "core/trader.hpp"
#include "api/alpaca_client.hpp"
#include "threads/logging_thread.hpp"
#include "threads/market_data_thread.hpp"
#include "threads/account_data_thread.hpp"
#include "threads/market_gate_thread.hpp"
#include "threads/trader_thread.hpp"
#include <iostream>
#include <csignal>
#include <memory>

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

TradingSystemModules create_trading_modules(const SystemConfig& config) {
    (void)config; // Suppress unused parameter warning
    return TradingSystemModules{};
}

void log_startup_information(const TradingSystemModules& modules, const SystemConfig& config) {
    StartupLogs::log_thread_system_startup(config.timing);
    // Add other startup logging as needed
}

void configure_trading_modules(SystemThreads& handles, TradingSystemModules& modules) {
    // Configure modules with iteration counters
    modules.trading_engine->set_iteration_counter(handles.trader_iterations);
    // Note: async_logger might not have set_iteration_counter method
    // modules.async_logger->set_iteration_counter(handles.logger_iterations);
}

SystemThreads SystemManager::startup(SystemState& system_state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger) {
    // Create handles for the threads
    SystemThreads handles;
    
    // Create all trading system modules and store them in system_state for lifetime management
    system_state.trading_modules = std::make_unique<TradingSystemModules>(create_trading_modules(system_state.config));
    
    // Log startup information
    log_startup_information(*system_state.trading_modules, system_state.config);
    
    // Configure trading modules
    configure_trading_modules(handles, *system_state.trading_modules);
    
    // Setup signal handling for graceful shutdown
    setup_signal_handlers(system_state);
    
    // Create thread configurations from single source
    auto& modules = *system_state.trading_modules;
    auto [thread_definitions, thread_infos] = Manager::create_thread_configurations(handles, modules);
    
    // Setup thread priorities
    Manager::setup_thread_priorities(thread_definitions, system_state.config);
    
    // Start all threads
    Manager::start_threads(thread_definitions);
    
    // Store thread infos for monitoring
    system_state.thread_infos = std::move(thread_infos);
    
    return handles;
}

static void run_until_shutdown(SystemState& state, SystemThreads& handles) {
    const int monitoring_interval_sec = state.config.timing.monitoring_interval_sec;
    auto last_monitoring_time = std::chrono::steady_clock::now();
    
    while (state.running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_monitoring_time).count();
        
        if (elapsed_seconds >= monitoring_interval_sec) {
            // Use the thread infos created from the same configuration source
            Manager::log_thread_monitoring_stats(state.thread_infos, handles.start_time);
            last_monitoring_time = current_time;
        }
    }
}

void SystemManager::shutdown(SystemState& system_state, SystemThreads& handles, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger) {
    // Signal all threads to stop
    system_state.cv.notify_all();
    
    // Wait for all threads to complete
    Manager::shutdown_threads();
    
    // Shutdown logging system
    AlpacaTrader::Logging::shutdown_global_logger(*logger);
}

std::vector<AlpacaTrader::Config::ThreadManagerConfig> SystemManager::create_thread_config_list(SystemThreads& handles, TradingSystemModules& modules) {
    std::vector<AlpacaTrader::Config::ThreadManagerConfig> configs;
    configs.reserve(5);
    
    // Market data processing thread
    configs.emplace_back(
        "Market Thread",
        [&modules]() { (*modules.market_data_thread)(); },
        handles.market_iterations,
        AlpacaTrader::Config::Type::MARKET_DATA
    );
    
    // Account data processing thread
    configs.emplace_back(
        "Account Thread",
        [&modules]() { (*modules.account_data_thread)(); },
        handles.account_iterations,
        AlpacaTrader::Config::Type::ACCOUNT_DATA
    );
    
    // Market gate control thread
    configs.emplace_back(
        "Gate Thread",
        [&modules]() { (*modules.market_gate_thread)(); },
        handles.gate_iterations,
        AlpacaTrader::Config::Type::MARKET_GATE
    );
    
    // Main trading logic thread
    configs.emplace_back(
        "Trader Thread",
        [&modules]() { (*modules.trading_thread)(); },
        handles.trader_iterations,
        AlpacaTrader::Config::Type::TRADER_DECISION
    );
    
    // Logging system thread
    configs.emplace_back(
        "Logger Thread",
        [&modules]() { (*modules.logging_thread)(); },
        handles.logger_iterations,
        AlpacaTrader::Config::Type::LOGGING
    );
    
    return configs;
}

void SystemManager::run(SystemState& system_state, SystemThreads& handles) {
    run_until_shutdown(system_state, handles);
}