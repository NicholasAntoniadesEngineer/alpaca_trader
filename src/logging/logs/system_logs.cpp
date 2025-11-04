#include "system_logs.hpp"
#include "logging/logger/async_logger.hpp"
#include <sstream>

using AlpacaTrader::Logging::log_message;

void SystemLogs::log_system_startup_error(const std::string& error_message) {
    log_message(std::string("ERROR: System startup error: ") + error_message, "system_logs");
}

void SystemLogs::log_system_shutdown_error(const std::string& error_message) {
    log_message(std::string("ERROR: System shutdown error: ") + error_message, "system_logs");
}

void SystemLogs::log_system_warning(const std::string& warning_message) {
    log_message(std::string("WARNING: ") + warning_message, "system_logs");
}

void SystemLogs::log_thread_startup_error(const std::string& error_message) {
    log_message(std::string("ERROR: Error starting threads: ") + error_message, "system_logs");
}

void SystemLogs::log_thread_priority_error(const std::string& error_message) {
    log_message(std::string("ERROR: Failed to set thread priorities: ") + error_message, "system_logs");
}

void SystemLogs::log_thread_monitoring_error(const std::string& error_message) {
    log_message(std::string("ERROR: Error logging thread monitoring stats: ") + error_message, "system_logs");
}

void SystemLogs::log_health_check_error(const std::string& error_message) {
    log_message(std::string("ERROR: Health check error: ") + error_message, "system_logs");
}

void SystemLogs::log_main_loop_error(const std::string& error_message) {
    log_message(std::string("ERROR: Error in main loop: ") + error_message, "system_logs");
}

void SystemLogs::log_fatal_error(const std::string& error_message) {
    log_message(std::string("FATAL: Fatal error in run_until_shutdown: ") + error_message, "system_logs");
}

void SystemLogs::log_running_flag_warning() {
    log_message("WARNING: running flag is false at start", "system_logs");
}

void SystemLogs::log_logging_context_error() {
    log_message("ERROR: Logging context not initialized - system must fail without context", "system_logs");
}

void SystemLogs::log_startup_complete() {
    log_message("SYSTEM_STARTUP: System startup completed successfully", "system_logs");
}

void SystemLogs::log_configuration_validated(bool valid) {
    if (valid) {
        log_message("CONFIG_VALIDATION: Configuration validated successfully", "system_logs");
    } else {
        log_message("CONFIG_VALIDATION: Configuration validation FAILED - system may not function correctly", "system_logs");
    }
}

void SystemLogs::log_threads_started(int expected_count, int actual_count) {
    if (actual_count == expected_count) {
        log_message("THREAD_STARTUP: All " + std::to_string(expected_count) + " threads started successfully", "system_logs");
    } else {
        log_message("THREAD_STARTUP: WARNING - Only " + std::to_string(actual_count) + 
                    " of " + std::to_string(expected_count) + " threads started", "system_logs");
    }
}

void SystemLogs::log_connectivity_issue() {
    log_message("CONNECTIVITY_ISSUE: API connectivity problem detected", "system_logs");
}

void SystemLogs::log_critical_error(const std::string& error_description) {
    log_message("CRITICAL_ERROR: " + error_description, "system_logs");
}

void SystemLogs::log_system_alert(const std::string& alert_message) {
    log_message("SYSTEM ALERT: " + alert_message, "system_logs");
}

void SystemLogs::log_health_report_table(bool overall_health, bool startup_complete, bool configuration_valid,
                                         bool all_threads_started, int active_thread_count, int connectivity_issues_count,
                                         int critical_errors_count, int uptime_seconds) {
    // Log table-formatted health report to system_logs file
    log_message("┌───────────────────┬──────────────────────────────────────────────────┐", "system_logs");
    log_message("│ System Health     │ Startup & Runtime Status                         │", "system_logs");
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "system_logs");
    
    // Overall Health
    std::string health_status = overall_health ? "HEALTHY" : "UNHEALTHY";
    log_message("│ Overall Health    │ " + health_status + std::string(49 - health_status.length(), ' ') + "│", "system_logs");
    
    // Startup Complete
    std::string startup_status = startup_complete ? "YES" : "NO";
    log_message("│ Startup Complete  │ " + startup_status + std::string(49 - startup_status.length(), ' ') + "│", "system_logs");
    
    // Configuration Valid
    std::string config_status = configuration_valid ? "YES" : "NO";
    log_message("│ Configuration     │ " + config_status + std::string(49 - config_status.length(), ' ') + "│", "system_logs");
    
    // All Threads Started
    std::string threads_status = all_threads_started ? "YES" : "NO";
    log_message("│ All Threads Start │ " + threads_status + std::string(49 - threads_status.length(), ' ') + "│", "system_logs");
    
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "system_logs");
    
    // Active Threads
    std::string active_threads_str = std::to_string(active_thread_count);
    log_message("│ Active Threads    │ " + active_threads_str + std::string(49 - active_threads_str.length(), ' ') + "│", "system_logs");
    
    // Connectivity Issues
    std::string connectivity_issues_str = std::to_string(connectivity_issues_count);
    log_message("│ Connectivity Issu │ " + connectivity_issues_str + std::string(49 - connectivity_issues_str.length(), ' ') + "│", "system_logs");
    
    // Critical Errors
    std::string critical_errors_str = std::to_string(critical_errors_count);
    log_message("│ Critical Errors   │ " + critical_errors_str + std::string(49 - critical_errors_str.length(), ' ') + "│", "system_logs");
    
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "system_logs");
    
    // System Uptime
    std::string uptime_str = std::to_string(uptime_seconds) + " seconds";
    log_message("│ System Uptime     │ " + uptime_str + std::string(49 - uptime_str.length(), ' ') + "│", "system_logs");
    
    log_message("└───────────────────┴──────────────────────────────────────────────────┘", "system_logs");
}

std::string SystemLogs::format_health_report_string(bool overall_health, bool startup_complete, bool configuration_valid,
                                                   bool all_threads_started, int active_thread_count, int connectivity_issues_count,
                                                   int critical_errors_count, int uptime_seconds) {
    std::ostringstream health_report_stream;
    health_report_stream << "=== SYSTEM HEALTH REPORT ===\n";
    health_report_stream << "Overall Health: " << (overall_health ? "HEALTHY" : "UNHEALTHY") << "\n";
    health_report_stream << "Startup Complete: " << (startup_complete ? "YES" : "NO") << "\n";
    health_report_stream << "Configuration Valid: " << (configuration_valid ? "YES" : "NO") << "\n";
    health_report_stream << "All Threads Started: " << (all_threads_started ? "YES" : "NO") << "\n";
    health_report_stream << "Active Threads: " << active_thread_count << "\n";
    health_report_stream << "Connectivity Issues: " << connectivity_issues_count << "\n";
    health_report_stream << "Critical Errors: " << critical_errors_count << "\n";
    health_report_stream << "System Uptime: " << uptime_seconds << " seconds\n";
    return health_report_stream.str();
}

std::string SystemLogs::format_health_report_error_string() {
    return "=== SYSTEM HEALTH REPORT ===\nError generating report\n";
}

void SystemLogs::log_health_report_generation_error(const std::string& error_description) {
    log_critical_error(std::string("Error generating health report: ") + error_description);
}

