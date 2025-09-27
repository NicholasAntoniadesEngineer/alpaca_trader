#ifndef THREAD_LOGGER_HPP
#define THREAD_LOGGER_HPP

#include "configs/timing_config.hpp"
#include <string>
#include <atomic>
#include <vector>
#include <chrono>

/**
 * High-performance thread monitoring and logging system.
 * Tracks thread lifecycle, priorities, and performance metrics.
 */
class ThreadLogger {
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
    
private:
    static std::string format_priority_status(const std::string& thread_name,
                                            const std::string& priority,
                                            bool success);
};

#endif // THREAD_LOGGER_HPP
