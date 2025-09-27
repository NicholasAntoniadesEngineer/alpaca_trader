#include "thread_logger.hpp"
#include "startup_logger.hpp"
#include "async_logger.hpp"
#include "logging_macros.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>

// Using declarations for cleaner code
using AlpacaTrader::Logging::log_message;

std::string ThreadLogger::format_priority_status(const std::string& thread_name,
                                                const std::string& priority,
                                                bool success) {
    std::ostringstream oss;
    oss << thread_name << ": " << priority << " priority [" << (success ? "OK" : "FAIL") << "]";
    return oss.str();
}

void ThreadLogger::log_system_startup(const TimingConfig& config) {
    StartupLogger::log_thread_system_startup(config);
}

void ThreadLogger::log_system_shutdown() {
    log_message("THREADS", "Thread system shutdown complete");
}


void ThreadLogger::log_thread_stopped(const std::string& thread_name) {
    log_message("THREAD", thread_name + " thread stopped");
}

void ThreadLogger::log_priority_assignment(const std::string& thread_name,
                                          const std::string& requested_priority,
                                          const std::string& actual_priority,
                                          bool success) {
    if (!success) {
        std::ostringstream oss;
        oss << "     |   " << thread_name << ": WARNING - requested " << requested_priority << ", got " << actual_priority;
        log_message(oss.str(), "");
    }
}

void ThreadLogger::log_thread_performance(const std::string& thread_name,
                                         unsigned long iterations,
                                         double cpu_usage) {
    
    std::ostringstream oss;
    oss << thread_name << " performance - Iterations: " << iterations;
    
    if (cpu_usage >= 0.0) {
        oss << " | CPU: " << std::fixed << std::setprecision(1) << cpu_usage << "%";
    }
    
    log_message("PERF", oss.str());
}

void ThreadLogger::log_thread_health(const std::string& thread_name, bool healthy, const std::string& details) {
    
    std::ostringstream oss;
    oss << thread_name << " health: " << (healthy ? "OK" : "ERROR");
    if (!details.empty()) {
        oss << " - " << details;
    }
    
    if (healthy) {
        log_message("HEALTH", oss.str());
    } else {
        log_message("HEALTH", oss.str());
    }
}

void ThreadLogger::log_system_performance_summary(unsigned long total_iterations) {
    
    std::ostringstream oss;
    oss << "System performance summary - Total iterations: " << total_iterations;
    
    log_message("SUMMARY", oss.str());
}


void ThreadLogger::log_thread_monitoring_stats(const std::vector<ThreadInfo>& thread_infos, 
                                              const std::chrono::steady_clock::time_point& start_time) {
    // Calculate total runtime
    auto current_time = std::chrono::steady_clock::now();
    auto runtime_duration = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
    double runtime_seconds = runtime_duration.count();
    
    // Calculate total iterations across all threads
    unsigned long total_iterations = 0;
    for (const auto& thread_info : thread_infos) {
        total_iterations += thread_info.iterations.load();
    }
    
    // Calculate performance rate
    double iterations_per_second = (runtime_seconds > 0) ? total_iterations / runtime_seconds : 0.0;
    
    // Use proper table macros for consistent formatting
    TABLE_HEADER_48("Thread Monitor", "Iteration Counts & Performance");
    
    // Log each thread's iteration count
    for (const auto& thread_info : thread_infos) {
        TABLE_ROW_48(thread_info.name, std::to_string(thread_info.iterations.load()) + " iterations");
    }
    
    TABLE_SEPARATOR_48();
    
    // Performance summary
    std::string runtime_display = std::to_string((int)runtime_seconds) + " seconds";
    TABLE_ROW_48("Runtime", runtime_display);
    
    std::string total_display = std::to_string(total_iterations) + " total";
    TABLE_ROW_48("Total Iterations", total_display);
    
    std::ostringstream rate_stream;
    rate_stream << std::fixed << std::setprecision(1) << iterations_per_second << "/sec";
    TABLE_ROW_48("Performance Rate", rate_stream.str());
    
    TABLE_FOOTER_48();
}
