// system_manager.cpp
#include "core/system_manager.hpp"
#include "core/system_state.hpp"
#include "core/system_threads.hpp"
#include "core/trading_system_modules.hpp"
#include "configs/system_config.hpp"
#include "configs/component_configs.hpp"
#include "threads/thread_manager.hpp"
#include "logging/startup_logger.hpp"
#include "logging/async_logger.hpp"
#include <chrono>
#include <csignal>
#include <thread>
#include <ctime>

// Using declarations for cleaner code

// =============================================================================
// SIGNAL HANDLING
// =============================================================================

static std::atomic<bool>* g_running_ptr;

static void setup_signal_handlers(std::atomic<bool>& running) {
    g_running_ptr = &running;
    std::signal(SIGINT, [](int){ g_running_ptr->store(false); });
    std::signal(SIGTERM, [](int){ g_running_ptr->store(false); });
}

// =============================================================================
// STARTUP LOGGING
// =============================================================================

static void log_startup_information(const TradingSystemModules& modules, const SystemConfig& config) {
    StartupLogger::log_account_overview(*modules.portfolio_manager);
    StartupLogger::log_financial_summary(*modules.portfolio_manager);
    StartupLogger::log_current_positions(*modules.portfolio_manager);
    StartupLogger::log_data_source_configuration(config);
    StartupLogger::log_runtime_configuration(config);
    StartupLogger::log_strategy_configuration(config);
}

// =============================================================================
// SYSTEM LIFECYCLE IMPLEMENTATION
// =============================================================================

namespace SystemManager {

SystemThreads startup(SystemState& system_state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger) {
    // Create all trading system modules and store them in system_state for lifetime management
    system_state.trading_modules = std::make_unique<TradingSystemModules>(create_trading_modules(system_state));
    
    // Log startup information
    log_startup_information(*system_state.trading_modules, system_state.config);
    
    // Configure trading modules
    configure_trading_modules(system_state, *system_state.trading_modules);
    
    // Setup signal handling for graceful shutdown
    setup_signal_handlers(system_state.running);
    
    // Setup and start all threads
    return ThreadSystem::setup_and_start_threads(*system_state.trading_modules, logger, system_state.config);
}

static void run_until_shutdown(SystemState& state, SystemThreads& handles) {
    const int monitoring_interval_sec = state.config.timing.monitoring_interval_sec;
    auto last_monitoring_time = std::chrono::steady_clock::now();
    
    while (state.running.load()) {
        // Sleep for 1 second instead of busy wait
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Check if it's time for monitoring output
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_monitoring_time).count();
        
        if (elapsed_seconds >= monitoring_interval_sec) {
            ThreadSystem::Manager::log_thread_monitoring_stats(handles);
            last_monitoring_time = current_time;
        }
    }
}

void run(SystemState& system_state, SystemThreads& handles) {
    run_until_shutdown(system_state, handles);
}

void shutdown(SystemState& system_state, SystemThreads& handles, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger) {
    // Signal all threads to stop
    system_state.cv.notify_all();
    
    // Wait for all threads to complete
    ThreadSystem::shutdown_system_threads(handles);
    
    // Shutdown logging system
    AlpacaTrader::Logging::shutdown_global_logger(*logger);
}

} // namespace SystemManager
