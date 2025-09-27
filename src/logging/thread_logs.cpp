#include "thread_logs.hpp"
#include "startup_logs.hpp"
#include "async_logger.hpp"
#include "logging_macros.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>

// Using declarations for cleaner code
using AlpacaTrader::Logging::log_message;

std::string ThreadLogs::format_priority_status(const std::string& thread_name,
                                                const std::string& priority,
                                                bool success) {
    std::ostringstream oss;
    oss << thread_name << ": " << priority << " priority [" << (success ? "OK" : "FAIL") << "]";
    return oss.str();
}

void ThreadLogs::log_system_startup(const TimingConfig& config) {
    StartupLogs::log_thread_system_startup(config);
}

void ThreadLogs::log_system_shutdown() {
    log_message("THREADS", "Thread system shutdown complete");
}


void ThreadLogs::log_thread_stopped(const std::string& thread_name) {
    log_message("THREAD", thread_name + " thread stopped");
}

void ThreadLogs::log_priority_assignment(const std::string& thread_name,
                                          const std::string& requested_priority,
                                          const std::string& actual_priority,
                                          bool success) {
    if (!success) {
        std::ostringstream oss;
        oss << "     |   " << thread_name << ": WARNING - requested " << requested_priority << ", got " << actual_priority;
        log_message(oss.str(), "");
    }
}

void ThreadLogs::log_thread_performance(const std::string& thread_name,
                                         unsigned long iterations,
                                         double cpu_usage) {
    
    std::ostringstream oss;
    oss << thread_name << " performance - Iterations: " << iterations;
    
    if (cpu_usage >= 0.0) {
        oss << " | CPU: " << std::fixed << std::setprecision(1) << cpu_usage << "%";
    }
    
    log_message("PERF", oss.str());
}

void ThreadLogs::log_thread_health(const std::string& thread_name, bool healthy, const std::string& details) {
    
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

void ThreadLogs::log_system_performance_summary(unsigned long total_iterations) {
    
    std::ostringstream oss;
    oss << "System performance summary - Total iterations: " << total_iterations;
    
    log_message("SUMMARY", oss.str());
}


void ThreadLogs::log_thread_monitoring_stats(const std::vector<ThreadInfo>& thread_infos, 
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

// Thread configuration logging methods
void ThreadLogs::log_thread_configuration_skipped(const std::string& thread_name, const std::string& reason) {
    log_message("THREAD_CONFIG", "Thread " + thread_name + " configuration skipped - " + reason);
}

void ThreadLogs::log_thread_cpu_affinity_configured(const std::string& thread_name, int cpu_core) {
    log_message("THREAD_CONFIG", "Thread " + thread_name + " configured for CPU core " + 
               std::to_string(cpu_core) + " with priority NORMAL");
}

void ThreadLogs::log_thread_priority_configured(const std::string& thread_name) {
    log_message("THREAD_CONFIG", "Thread " + thread_name + " configured with priority NORMAL");
}

void ThreadLogs::log_thread_cpu_affinity_failed(const std::string& thread_name) {
    log_message("THREAD_CONFIG", "Thread " + thread_name + " CPU affinity configuration failed, using fallback priority");
}

void ThreadLogs::log_thread_priority_failed(const std::string& thread_name) {
    log_message("THREAD_CONFIG", "Thread " + thread_name + " priority configuration failed");
}

void ThreadLogs::log_configuration_result(const std::string& thread_name, 
                                          const AlpacaTrader::Config::ThreadConfig& platform_config, 
                                          bool success) {
    if (success) {
        if (platform_config.cpu_affinity >= 0) {
            log_thread_cpu_affinity_configured(thread_name, platform_config.cpu_affinity);
        } else {
            log_thread_priority_configured(thread_name);
        }
    } else {
        if (platform_config.cpu_affinity >= 0) {
            log_thread_cpu_affinity_failed(thread_name);
        } else {
            log_thread_priority_failed(thread_name);
        }
    }
}
