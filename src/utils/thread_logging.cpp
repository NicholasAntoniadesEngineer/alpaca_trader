#include "utils/thread_logging.hpp"
#include "utils/async_logger.hpp"
#include "utils/thread_utils.hpp"
#include "main.hpp" // For SystemThreads definition
#include <string>

void ThreadLogging::log_startup_configuration(const TimingConfig& config) {
    if (!config.thread_priorities.log_thread_info) return;
    
    log_message("", "trade_log.txt");
    log_message("╔══════════════════════════════════════════════════════════════════════════════════╗", "trade_log.txt");
    log_message("║                         THREAD PRIORITY CONFIGURATION                           ║", "trade_log.txt");
    log_message("╚══════════════════════════════════════════════════════════════════════════════════╝", "trade_log.txt");
    log_message("", "trade_log.txt");
    log_message("   ┌─ CONFIGURATION:", "trade_log.txt");
    log_message("   |  • Thread priorities: " + std::string(config.thread_priorities.enable_thread_priorities ? "ENABLED" : "DISABLED"), "trade_log.txt");
    log_message("   |  • CPU affinity: " + std::string(config.thread_priorities.enable_cpu_affinity ? "ENABLED" : "DISABLED"), "trade_log.txt");
    
    if (config.thread_priorities.enable_cpu_affinity) {
        log_message("      |  • Trader CPU binding: CPU " + std::to_string(config.thread_priorities.trader_cpu_affinity), "trade_log.txt");
        log_message("      |  • Market data CPU binding: CPU " + std::to_string(config.thread_priorities.market_data_cpu_affinity), "trade_log.txt");
    } else {
        log_message("   |  • No CPU affinity binding", "trade_log.txt");
    }
    log_message("   └─", "trade_log.txt");

    log_message("", "trade_log.txt");
    log_message("   ┌─ THREAD INITIALIZATION:", "trade_log.txt");
    log_message("   |  • Main Thread: " + ThreadUtils::get_thread_info(), "trade_log.txt");
}

void ThreadLogging::log_thread_initialization_complete() {
    log_message("   └─", "trade_log.txt");
    log_message("", "trade_log.txt");
}

void ThreadLogging::log_priority_setup_header() {
    log_message("   ┌─ PRIORITY ASSIGNMENT:", "trade_log.txt");
}

void ThreadLogging::log_trader_priority_result(const std::string& requested_priority, const std::string& actual_priority, bool success, bool has_affinity, int cpu_affinity) {
    std::string affinity_info = has_affinity ? " → CPU " + std::to_string(cpu_affinity) : "";
    std::string priority_info = (requested_priority != actual_priority) ? 
        requested_priority + " (scaled to " + actual_priority + ")" : actual_priority;
    log_message(create_priority_status("TRADER", priority_info, success, "[CRITICAL]", affinity_info), "trade_log.txt");
}

void ThreadLogging::log_market_priority_result(const std::string& requested_priority, const std::string& actual_priority, bool success, bool has_affinity, int cpu_affinity) {
    std::string affinity_info = has_affinity ? " → CPU " + std::to_string(cpu_affinity) : "";
    std::string priority_info = (requested_priority != actual_priority && success) ? 
        requested_priority + " (scaled to " + actual_priority + ")" : actual_priority;
    log_message(create_priority_status("MARKET", priority_info, success, "[HIGH]", affinity_info), "trade_log.txt");
}

void ThreadLogging::log_account_priority_result(const std::string& requested_priority, const std::string& actual_priority, bool success) {
    std::string priority_info = (requested_priority != actual_priority && success) ? 
        requested_priority + " (scaled to " + actual_priority + ")" : actual_priority;
    log_message(create_priority_status("ACCOUNT", priority_info, success, "[NORMAL]"), "trade_log.txt");
}

void ThreadLogging::log_gate_priority_result(const std::string& requested_priority, const std::string& actual_priority, bool success) {
    std::string priority_info = (requested_priority != actual_priority && success) ? 
        requested_priority + " (scaled to " + actual_priority + ")" : actual_priority;
    log_message(create_priority_status("GATE", priority_info, success, "[LOW]"), "trade_log.txt");
}

void ThreadLogging::log_priority_setup_footer() {
    log_message("   └─", "trade_log.txt");
    log_message("", "trade_log.txt");
}

void ThreadLogging::log_performance_summary(const SystemThreads& handles) {
    auto elapsed = std::chrono::steady_clock::now() - handles.start_time;
    auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    
    if (elapsed_sec > 0) {
        log_message("", "trade_log.txt");
        log_message("╔══════════════════════════════════════════════════════════════════════════════════╗", "trade_log.txt");
        log_message("║                            THREAD PERFORMANCE SUMMARY                           ║", "trade_log.txt");
        log_message("╚══════════════════════════════════════════════════════════════════════════════════╝", "trade_log.txt");
        log_message("", "trade_log.txt");
        log_message("   • Runtime Statistics:", "trade_log.txt");
        log_message("      └─ Total runtime: " + std::to_string(elapsed_sec) + " seconds", "trade_log.txt");
        log_message("", "trade_log.txt");
        log_message("   • Thread Performance:", "trade_log.txt");
        
        log_message("      ├─ [CRITICAL] TRADER: " + format_number(handles.trader_iterations.load()) + 
                   " iterations (" + std::to_string(handles.trader_iterations.load() / elapsed_sec) + "/sec)", "trade_log.txt");
        log_message("      ├─ [HIGH]     MARKET: " + format_number(handles.market_iterations.load()) + 
                   " iterations (" + std::to_string(handles.market_iterations.load() / elapsed_sec) + "/sec)", "trade_log.txt");
        log_message("      ├─ [NORMAL]   ACCOUNT: " + format_number(handles.account_iterations.load()) + 
                   " iterations (" + std::to_string(handles.account_iterations.load() / elapsed_sec) + "/sec)", "trade_log.txt");
        log_message("      └─ [LOW]      GATE: " + format_number(handles.gate_iterations.load()) + 
                   " iterations (" + std::to_string(handles.gate_iterations.load() / elapsed_sec) + "/sec)", "trade_log.txt");
        log_message("", "trade_log.txt");
    }
}

void ThreadLogging::log_thread_started(const std::string& thread_name, const std::string& thread_info) {
    log_message("   • " + thread_name + " thread started: " + thread_info, "");
}

void ThreadLogging::log_prioritization_disabled() {
    log_message("   • WARNING: Thread prioritization disabled in configuration", "trade_log.txt");
}

std::string ThreadLogging::format_number(unsigned long num) {
    std::string str = std::to_string(num);
    int n = str.length() - 3;
    while (n > 0) {
        str.insert(n, ",");
        n -= 3;
    }
    return str;
}

std::string ThreadLogging::create_priority_status(const std::string& thread_name, 
                                                 const std::string& priority_level, 
                                                 bool success, 
                                                 const std::string& priority_tag,
                                                 const std::string& affinity_info) {
    std::string status = success ? " [OK]" : " [FAILED]";
    
    return "   |  • " + priority_tag + " " + thread_name + ": " + 
           priority_level + " priority" + affinity_info + status;
}
