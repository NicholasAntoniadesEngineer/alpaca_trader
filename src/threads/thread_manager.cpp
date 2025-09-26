#include "threads/thread_manager.hpp"
#include "threads/config/thread_config.hpp"
#include "threads/platform/thread_control.hpp"
#include "logging/thread_logger.hpp"
#include "logging/startup_logger.hpp"
#include "logging/async_logger.hpp"
#include "logging/logging_macros.hpp"
#include "configs/system_config.hpp"  
#include "core/system_threads.hpp"
#include "core/trading_system_modules.hpp"
#include "threads/logging_thread.hpp"
#include "threads/trader_thread.hpp"
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>

// Using declarations for cleaner code
using AlpacaTrader::Logging::log_message;
using AlpacaTrader::Logging::AsyncLogger;
using AlpacaTrader::Threads::LoggingThread;
using AlpacaTrader::Threads::TraderThread;

namespace ThreadSystem {

// Static member definition
std::vector<std::tuple<std::string, std::string, bool>> Manager::thread_status_data;

// Configures thread priority and CPU affinity
void Manager::configure_single_thread(const ThreadSetup& setup, const TimingConfig& config) {
    // Get default configuration for this thread type
    ThreadConfig thread_config = ConfigProvider::get_default_config(setup.type);
    
    // Apply CPU affinity if enabled and this thread uses it
    if (config.thread_priorities.enable_cpu_affinity && setup.uses_cpu_affinity) {
        thread_config.cpu_affinity = setup.cpu_core;
    }
    
    // Apply priority with fallback scaling
    Priority requested_priority = thread_config.priority;
    Priority actual_priority = Platform::ThreadControl::set_priority_with_fallback(setup.handle, thread_config);
    
    // Determine success based on whether we achieved the goal
    bool success = (actual_priority == requested_priority) || (thread_config.cpu_affinity < 0);
    
    // Store thread status data for dynamic table logging
    thread_status_data.push_back(std::make_tuple(
        setup.name,
        ConfigProvider::priority_to_string(actual_priority),
        success
    ));
    
    // Log the result using the generic logging function
    ThreadLogger::log_priority_assignment(
        setup.name,
        ConfigProvider::priority_to_string(requested_priority),
        ConfigProvider::priority_to_string(actual_priority),
        success
    );
}

std::vector<std::tuple<std::string, std::string, bool>> Manager::get_thread_status_data() {
    return thread_status_data;
}

void Manager::setup_thread_priorities(SystemThreads& handles, const TimingConfig& config) {
    // Clear any previous thread status data
    thread_status_data.clear();
    
    if (!config.thread_priorities.enable_thread_priorities) {
        // Thread prioritization disabled - logged by system startup
        return;
    }
    
    std::vector<ThreadSetup> thread_setups = {
        ThreadSetup("TRADER",  Type::TRADER_DECISION, handles.trader, "[CRITICAL]", true, config.thread_priorities.trader_cpu_affinity),
        ThreadSetup("MARKET",  Type::MARKET_DATA,     handles.market, "[HIGH]",     true, config.thread_priorities.market_data_cpu_affinity),
        ThreadSetup("ACCOUNT", Type::ACCOUNT_DATA,    handles.account,"[NORMAL]",   false, -1),
        ThreadSetup("GATE",    Type::MARKET_GATE,     handles.gate,   "[LOW]",      false, -1),
        ThreadSetup("LOGGER",  Type::LOGGING,         handles.logger, "[LOWEST]",   false, -1)
    };
    
    // Configure each thread using the structured approach
    for (const auto& setup : thread_setups) {
        configure_single_thread(setup, config);
    }
    
    // Close the thread system section
    StartupLogger::log_thread_system_complete();
}

void Manager::log_thread_startup_info(const TimingConfig& config) {
    ThreadLogger::log_system_startup(config);
}

void Manager::log_thread_monitoring_stats(const SystemThreads& handles) {
    // Calculate total runtime
    auto current_time = std::chrono::steady_clock::now();
    auto runtime_duration = std::chrono::duration_cast<std::chrono::seconds>(current_time - handles.start_time);
    double runtime_seconds = runtime_duration.count();
    
    // Calculate total iterations across all threads
    unsigned long total_iterations = 
        handles.market_iterations.load() +
        handles.account_iterations.load() +
        handles.gate_iterations.load() +
        handles.trader_iterations.load() +
        handles.logger_iterations.load();
    
    // Calculate performance rate
    double iterations_per_second = (runtime_seconds > 0) ? total_iterations / runtime_seconds : 0.0;
    
    // Use proper table macros for consistent formatting
    TABLE_HEADER_48("Thread Monitor", "Iteration Counts & Performance");
    
    TABLE_ROW_48("Market Thread", std::to_string(handles.market_iterations.load()) + " iterations");
    TABLE_ROW_48("Account Thread", std::to_string(handles.account_iterations.load()) + " iterations");
    TABLE_ROW_48("Trader Thread", std::to_string(handles.trader_iterations.load()) + " iterations");
    TABLE_ROW_48("Gate Thread", std::to_string(handles.gate_iterations.load()) + " iterations");
    TABLE_ROW_48("Logger Thread", std::to_string(handles.logger_iterations.load()) + " iterations");
    
    TABLE_SEPARATOR_48();
    
    // Performance summary
    std::string runtime_display = std::to_string((int)runtime_seconds) + " seconds";
    TABLE_ROW_48("Runtime", runtime_display);
    
    std::string total_display = std::to_string(total_iterations) + " total";
    TABLE_ROW_48("Total Iterations", total_display);
    
    std::ostringstream rate_stream;
    rate_stream << std::fixed << std::setprecision(1) << iterations_per_second << "/sec";
    TABLE_ROW_48("Performance Rate", rate_stream.str());
    
    TABLE_FOOTER_48();
}

// =============================================================================
// THREAD SETUP AND LIFECYCLE MANAGEMENT
// =============================================================================

SystemThreads setup_and_start_threads(TradingSystemModules& modules, std::shared_ptr<AsyncLogger> logger, const SystemConfig& config) {
    SystemThreads handles;
    
    // Create remaining thread objects
    modules.logging_thread = std::make_unique<LoggingThread>(
        logger, handles.logger_iterations
    );
    modules.trading_thread = std::make_unique<TraderThread>(
        *modules.trading_engine, handles.trader_iterations
    );
    
    // Set iteration counters for all threads
    modules.market_data_thread->set_iteration_counter(handles.market_iterations);
    modules.account_data_thread->set_iteration_counter(handles.account_iterations);
    modules.market_gate_thread->set_iteration_counter(handles.gate_iterations);
    
    // Log thread system startup information
    StartupLogger::log_thread_system_startup(config.timing);
    
    // Start all threads
    handles.market = std::thread(std::ref(*modules.market_data_thread));
    handles.account = std::thread(std::ref(*modules.account_data_thread));
    handles.gate = std::thread(std::ref(*modules.market_gate_thread));
    handles.trader = std::thread(std::ref(*modules.trading_thread));
    handles.logger = std::thread(std::ref(*modules.logging_thread));
    
    // Allow threads to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Configure thread priorities
    Manager::setup_thread_priorities(handles, config.timing);
    
    return handles;
}

void shutdown_system_threads(SystemThreads& handles) {
    // Wait for all threads to complete
    if (handles.market.joinable()) handles.market.join();
    if (handles.account.joinable()) handles.account.join();
    if (handles.gate.joinable()) handles.gate.join();
    if (handles.trader.joinable()) handles.trader.join();
    if (handles.logger.joinable()) handles.logger.join();
}

} // namespace ThreadSystem

