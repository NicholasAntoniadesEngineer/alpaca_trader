#include "thread_manager.hpp"
#include "config/thread_config.hpp"
#include "platform/thread_control.hpp"
#include "../logging/thread_logger.hpp"
#include "../logging/startup_logger.hpp"
#include "../logging/async_logger.hpp"
#include "../logging/logging_macros.hpp"
#include "../main.hpp"  // For SystemThreads definition
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>

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
    
    // Log thread status table
    LOG_THREAD_STATUS_HEADER();
    LOG_THREAD_STATUS_TABLE_HEADER();
    LOG_THREAD_STATUS_TABLE_COLUMNS();
    LOG_THREAD_STATUS_TABLE_SEPARATOR();
    
    // Build table row with iteration counts
    std::ostringstream table_row;
    table_row << "| " << std::setw(8) << std::left << handles.market_iterations.load()
              << " | " << std::setw(8) << std::left << handles.account_iterations.load() 
              << " | " << std::setw(8) << std::left << handles.trader_iterations.load()
              << " | " << std::setw(8) << std::left << handles.gate_iterations.load()
              << " | " << std::setw(8) << std::left << handles.logger_iterations.load() << " |";
    LOG_THREAD_CONTENT(table_row.str());
    
    LOG_THREAD_STATUS_TABLE_FOOTER();
    LOG_THREAD_SEPARATOR();
    
    // Log performance summary
    std::ostringstream summary;
    summary << "Total Iterations: " << total_iterations << "  |  Rate: " 
            << std::fixed << std::setprecision(1) << iterations_per_second << "/s";
    LOG_THREAD_CONTENT(summary.str());
}

} // namespace ThreadSystem

