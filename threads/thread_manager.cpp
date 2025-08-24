#include "thread_manager.hpp"
#include "config/thread_config.hpp"
#include "platform/thread_control.hpp"
#include "../utils/thread_logging.hpp"
#include "../main.hpp"  // For SystemThreads definition
#include <vector>

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
    ThreadLogging::log_priority_result(
        setup.name, 
        setup.priority_tag,
        ConfigProvider::priority_to_string(requested_priority),
        ConfigProvider::priority_to_string(actual_priority),
        success,
        thread_config.cpu_affinity >= 0,
        thread_config.cpu_affinity
    );
}

void Manager::setup_thread_priorities(SystemThreads& handles, const TimingConfig& config) {
    if (!config.thread_priorities.enable_thread_priorities) {
        ThreadLogging::log_prioritization_disabled();
        return;
    }
    
    ThreadLogging::log_priority_setup_header();
    
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
    
    ThreadLogging::log_priority_setup_footer();
}

void Manager::log_thread_startup_info(const TimingConfig& config) {
    ThreadLogging::log_startup_configuration(config);
}

void Manager::log_thread_monitoring_stats(const SystemThreads& handles) {
    ThreadLogging::log_performance_summary(handles);
}

} // namespace ThreadSystem
