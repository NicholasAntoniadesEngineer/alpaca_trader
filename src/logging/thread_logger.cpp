#include "thread_logger.hpp"
#include "startup_logger.hpp"
#include "async_logger.hpp"
#include "logging_macros.hpp"
#include <sstream>
#include <iomanip>

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
