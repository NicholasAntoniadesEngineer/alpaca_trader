#ifndef SYSTEM_LOGS_HPP
#define SYSTEM_LOGS_HPP

#include <string>

/**
 * Specialized logging for system management operations.
 * Handles all system-level logging in a consistent format.
 */
class SystemLogs {
public:
    // System startup and shutdown
    static void log_system_startup_error(const std::string& error_message);
    static void log_system_shutdown_error(const std::string& error_message);
    static void log_system_warning(const std::string& warning_message);
    
    // Thread management errors
    static void log_thread_startup_error(const std::string& error_message);
    static void log_thread_priority_error(const std::string& error_message);
    static void log_thread_monitoring_error(const std::string& error_message);
    
    // System health monitoring
    static void log_health_check_error(const std::string& error_message);
    static void log_main_loop_error(const std::string& error_message);
    static void log_fatal_error(const std::string& error_message);
    
    // System state errors
    static void log_running_flag_warning();
    static void log_logging_context_error();
    
    // System monitor events
    static void log_startup_complete();
    static void log_configuration_validated(bool valid);
    static void log_threads_started(int expected_count, int actual_count);
    static void log_connectivity_issue();
    static void log_critical_error(const std::string& error_description);
    static void log_system_alert(const std::string& alert_message);
};

#endif // SYSTEM_LOGS_HPP

