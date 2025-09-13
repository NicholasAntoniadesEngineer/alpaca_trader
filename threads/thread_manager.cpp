#include "thread_manager.hpp"
#include "config/thread_config.hpp"
#include "platform/thread_control.hpp"
#include "../logging/thread_logger.hpp"
#include "../logging/async_logger.hpp"
#include "../main.hpp"  // For SystemThreads definition
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace ThreadSystem {

// Helper function to configure a single thread with consistent logic
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
    
    // Log the result using the generic logging function
    ThreadLogger::log_priority_assignment(
        setup.name,
        ConfigProvider::priority_to_string(requested_priority),
        ConfigProvider::priority_to_string(actual_priority),
        success
    );
}

void Manager::setup_thread_priorities(SystemThreads& handles, const TimingConfig& config) {
    if (!config.thread_priorities.enable_thread_priorities) {
        // Thread prioritization disabled - logged by system startup
        return;
    }
    
    // Priority setup header logged by centralized system
    
    // ╔═══════════════════════════════════════════════════════════════════════════════════╗
    // ║                           THREAD CONFIGURATION TABLE                             ║
    // ╠═══════════════════╦══════════════════╦══════════════╦═════════════╦═════════════╣
    // ║ Thread Name       ║ Type             ║ Priority     ║ CPU Affinity║ Criticality ║
    // ╠═══════════════════╬══════════════════╬══════════════╬═════════════╬═════════════╣
    // ║ TRADER            ║ TRADER_DECISION  ║ HIGHEST      ║ Configurable║ CRITICAL    ║
    // ║ MARKET            ║ MARKET_DATA      ║ HIGH         ║ Configurable║ HIGH        ║
    // ║ ACCOUNT           ║ ACCOUNT_DATA     ║ NORMAL       ║ None        ║ NORMAL      ║
    // ║ GATE              ║ MARKET_GATE      ║ LOW          ║ None        ║ LOW         ║
    // ║ LOGGER            ║ LOGGING          ║ LOWEST       ║ None        ║ LOWEST      ║
    // ╚═══════════════════╩══════════════════╩══════════════╩═════════════╩═════════════╝
    
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
    
    // Priority setup footer logged by centralized system
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
    
    // Create compact multi-line summary
    std::ostringstream msg;
    msg << std::fixed << std::setprecision(0);
    msg << "THREADS STATUS:";
    msg << "\n                               Market:   " << handles.market_iterations.load();
    msg << ",    Account:  " << handles.account_iterations.load();
    msg << "\n                               Trader:   " << handles.trader_iterations.load();
    msg << ",    Gate:     " << handles.gate_iterations.load();
    msg << "\n                               Logger:   " << handles.logger_iterations.load();
    msg << ",   Total:    " << total_iterations;
    msg << "\n                               Rate:     " << std::fixed << std::setprecision(1) << iterations_per_second << "/s";
    
    // Log compact summary
    log_message(msg.str(), "");
}

} // namespace ThreadSystem

