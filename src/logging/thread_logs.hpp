#ifndef THREAD_LOGS_HPP
#define THREAD_LOGS_HPP

#include "configs/timing_config.hpp"
#include "configs/thread_config.hpp"
#include <string>
#include <atomic>
#include <vector>
#include <chrono>

/**
 * High-performance thread monitoring and logging system.
 * Tracks thread lifecycle, priorities, and performance metrics.
 */
class ThreadLogs {
public:
    // Thread system initialization
    static void log_system_startup(const TimingConfig& config);
    static void log_system_shutdown();
    
    // Thread lifecycle
    static void log_thread_stopped(const std::string& thread_name);
    
    // Priority management
    static void log_priority_assignment(const std::string& thread_name, 
                                      const std::string& requested_priority,
                                      const std::string& actual_priority,
                                      bool success);
    
    
    // Performance monitoring
    static void log_thread_performance(const std::string& thread_name,
                                     unsigned long iterations,
                                     double cpu_usage = -1.0);
    
    // System health
    static void log_thread_health(const std::string& thread_name, bool healthy, const std::string& details = "");
    static void log_system_performance_summary(unsigned long total_iterations);
    
    // Thread information structure for generic monitoring
    struct ThreadInfo {
        std::string name;
        std::atomic<unsigned long>& iterations;
        
        ThreadInfo(const std::string& thread_name, std::atomic<unsigned long>& iter_count)
            : name(thread_name), iterations(iter_count) {}
    };
    
    
    // Generic thread monitoring for any number of threads
    static void log_thread_monitoring_stats(const std::vector<ThreadInfo>& thread_infos, 
                                          const std::chrono::steady_clock::time_point& start_time);
    
    // Thread status table logging
    static void log_thread_status_table(const std::vector<AlpacaTrader::Config::ThreadStatusData>& thread_status_data);
    
    // Thread registry error handling
    static void log_thread_registry_error(const std::string& error_msg);
    static void log_thread_registry_warning(const std::string& warning_msg);
    static void log_thread_registry_validation_success(const std::string& details);
    
    // Thread exception handling
    static void log_thread_exception(const std::string& thread_name, const std::string& exception_msg);
    static void log_thread_unknown_exception(const std::string& thread_name);
    
    // Thread configuration errors
    static void log_thread_config_error(const std::string& thread_name, const std::string& error_msg);
    static void log_thread_config_warning(const std::string& thread_name, const std::string& warning_msg);
    
    // Thread lifecycle errors
    static void log_thread_startup_error(const std::string& thread_name, const std::string& error_msg);
    static void log_thread_shutdown_error(const std::string& thread_name, const std::string& error_msg);
    
    // Centralized error message construction
    static std::string build_unknown_thread_type_error(const std::string& type_name, int enum_value);
    static std::string build_unknown_thread_identifier_error(const std::string& identifier);
    static std::string build_unknown_thread_type_enum_error(int enum_value);
    static std::string build_duplicate_thread_type_error(int enum_value);
    static std::string build_duplicate_thread_identifier_error(const std::string& identifier);
    static std::string build_empty_thread_identifier_error(int enum_value);
    
};

#endif // THREAD_LOGS_HPP
