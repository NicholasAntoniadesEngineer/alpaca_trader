// system_manager.cpp
#include "system_manager.hpp"
#include "../main.hpp"
#include "../configs/component_configs.hpp"
#include "../threads/thread_manager.hpp"
#include "../logging/startup_logger.hpp"
#include <chrono>
#include <csignal>
#include <thread>

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
}

// =============================================================================
// SYSTEM LIFECYCLE IMPLEMENTATION
// =============================================================================

namespace SystemManager {

SystemThreads startup(SystemState& system_state, AsyncLogger& logger) {
    // Create all trading system modules
    TradingSystemModules trading_modules = create_trading_modules(system_state);
    
    // Log startup information
    log_startup_information(trading_modules, system_state.config);
    
    // Configure trading modules
    configure_trading_modules(system_state, trading_modules);
    
    // Setup signal handling for graceful shutdown
    setup_signal_handlers(system_state.running);
    
    // Setup and start all threads
    return ThreadSystem::setup_and_start_threads(trading_modules, logger, system_state.config);
}

static void run_until_shutdown(SystemState& state, SystemThreads& handles) {
    auto last_monitoring_time = std::chrono::steady_clock::now();
    const auto monitoring_interval = std::chrono::seconds(state.config.timing.monitoring_interval_sec);
    
    while (state.running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Periodic monitoring
        auto current_time = std::chrono::steady_clock::now();
        if (current_time - last_monitoring_time >= monitoring_interval) {
            ThreadSystem::Manager::log_thread_monitoring_stats(handles);
            last_monitoring_time = current_time;
        }
    }
}

void run(SystemState& system_state, SystemThreads& handles) {
    run_until_shutdown(system_state, handles);
}

void shutdown(SystemState& system_state, SystemThreads& handles, AsyncLogger& logger) {
    // Signal all threads to stop
    system_state.cv.notify_all();
    
    // Wait for all threads to complete
    ThreadSystem::shutdown_system_threads(handles);
    
    // Shutdown logging system
    shutdown_global_logger(logger);
}

} // namespace SystemManager
