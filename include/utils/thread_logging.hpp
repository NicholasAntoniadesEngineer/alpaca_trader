#ifndef THREAD_LOGGING_HPP
#define THREAD_LOGGING_HPP

#include "../configs/timing_config.hpp"
#include <string>
#include <chrono>
#include <atomic>

// Forward declarations to avoid circular dependencies
struct SystemThreads;

class ThreadLogging {
public:
    // Log the initial thread configuration at startup
    static void log_startup_configuration(const TimingConfig& config);
    
    // Log individual thread priority setup results
    static void log_priority_setup_header();
    static void log_thread_initialization_complete();
    static void log_trader_priority_result(const std::string& requested_priority, const std::string& actual_priority, bool success, bool has_affinity, int cpu_affinity);
    static void log_market_priority_result(const std::string& requested_priority, const std::string& actual_priority, bool success, bool has_affinity, int cpu_affinity);
    static void log_account_priority_result(const std::string& requested_priority, const std::string& actual_priority, bool success);
    static void log_gate_priority_result(const std::string& requested_priority, const std::string& actual_priority, bool success);
    static void log_priority_setup_footer();
    
    // Log thread performance statistics at shutdown
    static void log_performance_summary(const SystemThreads& handles);
    
    // Log thread startup messages from individual threads
    static void log_thread_started(const std::string& thread_name, const std::string& thread_info);
    
    // Log when thread prioritization is disabled
    static void log_prioritization_disabled();

private:
    // Helper function to format large numbers with commas
    static std::string format_number(unsigned long num);
    
    // Helper function to create priority status message
    static std::string create_priority_status(const std::string& thread_name, 
                                            const std::string& priority_level, 
                                            bool success, 
                                            const std::string& priority_tag,
                                            const std::string& affinity_info = "");
};

#endif // THREAD_LOGGING_HPP
