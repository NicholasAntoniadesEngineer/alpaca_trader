#include "utils/thread_manager.hpp"
#include "utils/thread_logging.hpp"
#include "main.hpp" // For SystemThreads definition
#include <string>

void ThreadManager::setup_thread_priorities(SystemThreads& handles, const TimingConfig& config) {
    if (!config.thread_priorities.enable_thread_priorities) {
        ThreadLogging::log_prioritization_disabled();
        return;
    }

    ThreadLogging::log_thread_initialization_complete();
    ThreadLogging::log_priority_setup_header();

    // Setup trader thread (highest priority with fallback)
    ThreadConfig trader_config = ThreadUtils::get_default_config(ThreadType::TRADER_DECISION);
    if (config.thread_priorities.enable_cpu_affinity) {
        trader_config.cpu_affinity = config.thread_priorities.trader_cpu_affinity;
    }
    
    ThreadPriority requested_trader_priority = trader_config.priority;
    ThreadPriority actual_trader_priority = ThreadUtils::set_thread_priority_with_fallback(handles.trader, trader_config);
    // Success if we got the requested priority OR if no CPU affinity was requested
    bool trader_success = (actual_trader_priority == requested_trader_priority) || (trader_config.cpu_affinity < 0);
    
    ThreadLogging::log_trader_priority_result(ThreadUtils::priority_to_string(requested_trader_priority),
                                             ThreadUtils::priority_to_string(actual_trader_priority),
                                             trader_success, 
                                             trader_config.cpu_affinity >= 0, 
                                             trader_config.cpu_affinity);

    // Setup market data thread (high priority with fallback)
    ThreadConfig market_config = ThreadUtils::get_default_config(ThreadType::MARKET_DATA);
    if (config.thread_priorities.enable_cpu_affinity) {
        market_config.cpu_affinity = config.thread_priorities.market_data_cpu_affinity;
    }
    
    ThreadPriority requested_market_priority = market_config.priority;
    ThreadPriority actual_market_priority = ThreadUtils::set_thread_priority_with_fallback(handles.market, market_config);
    // Success if we got the requested priority OR if no CPU affinity was requested
    bool market_success = (actual_market_priority == requested_market_priority) || (market_config.cpu_affinity < 0);
    
    ThreadLogging::log_market_priority_result(ThreadUtils::priority_to_string(requested_market_priority),
                                             ThreadUtils::priority_to_string(actual_market_priority),
                                             market_success,
                                             market_config.cpu_affinity >= 0,
                                             market_config.cpu_affinity);

    // Setup account thread (normal priority with fallback)
    ThreadConfig account_config = ThreadUtils::get_default_config(ThreadType::ACCOUNT_DATA);
    ThreadPriority requested_account_priority = account_config.priority;
    ThreadPriority actual_account_priority = ThreadUtils::set_thread_priority_with_fallback(handles.account, account_config);
    // Success if we got the requested priority (account has no CPU affinity by default)
    bool account_success = true;
    
    ThreadLogging::log_account_priority_result(ThreadUtils::priority_to_string(requested_account_priority),
                                              ThreadUtils::priority_to_string(actual_account_priority),
                                              account_success);

    // Setup gate thread (low priority with fallback)
    ThreadConfig gate_config = ThreadUtils::get_default_config(ThreadType::MARKET_GATE);
    ThreadPriority requested_gate_priority = gate_config.priority;
    ThreadPriority actual_gate_priority = ThreadUtils::set_thread_priority_with_fallback(handles.gate, gate_config);
    // Success if we got the requested priority (gate has no CPU affinity by default)
    bool gate_success = true;
    
    ThreadLogging::log_gate_priority_result(ThreadUtils::priority_to_string(requested_gate_priority),
                                           ThreadUtils::priority_to_string(actual_gate_priority),
                                           gate_success);
    
    ThreadLogging::log_priority_setup_footer();
}

void ThreadManager::log_thread_startup_info(const TimingConfig& config) {
    ThreadLogging::log_startup_configuration(config);
}

void ThreadManager::log_thread_monitoring_stats(const SystemThreads& handles) {
    ThreadLogging::log_performance_summary(handles);
}
