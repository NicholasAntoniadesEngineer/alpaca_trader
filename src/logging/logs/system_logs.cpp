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

