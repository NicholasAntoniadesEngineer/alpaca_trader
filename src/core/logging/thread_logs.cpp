#include "thread_logs.hpp"
#include "startup_logs.hpp"
#include "async_logger.hpp"
#include "logging_macros.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>

// Using declarations for cleaner code
using AlpacaTrader::Logging::log_message;


void ThreadLogs::log_system_startup(const TimingConfig& config) {
    StartupLogs::log_thread_system_startup(config);
}

void ThreadLogs::log_system_shutdown() {
    log_message("Thread system shutdown complete", "");
}


void ThreadLogs::log_thread_stopped(const std::string& thread_name) {
    log_message(thread_name + " thread stopped", "");
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
    
    log_message(oss.str(), "");
}

void ThreadLogs::log_thread_health(const std::string& thread_name, bool healthy, const std::string& details) {
    
    std::ostringstream oss;
    oss << thread_name << " health: " << (healthy ? "OK" : "ERROR");
    if (!details.empty()) {
        oss << " - " << details;
    }
    
    if (healthy) {
        log_message(oss.str(), "");
    } else {
        log_message(oss.str(), "");
    }
}

void ThreadLogs::log_system_performance_summary(unsigned long total_iterations) {
    
    std::ostringstream oss;
    oss << "System performance summary - Total iterations: " << total_iterations;
    
    log_message(oss.str(), "");
}


void ThreadLogs::log_thread_monitoring_stats(const std::vector<ThreadInfo>& thread_infos, 
                                              const std::chrono::steady_clock::time_point& start_time) {
    try {
        // Calculate total runtime
        auto current_time = std::chrono::steady_clock::now();
        auto runtime_duration = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
        double runtime_seconds = runtime_duration.count();
        
        // Calculate total iterations across all threads with error handling
        unsigned long total_iterations = 0;
        for (const auto& thread_info : thread_infos) {
            try {
                total_iterations += thread_info.iterations.load();
            } catch (const std::exception& e) {
                // Skip invalid thread info and continue
                continue;
            }
        }
        
        // Calculate performance rate
        double iterations_per_second = (runtime_seconds > 0) ? total_iterations / runtime_seconds : 0.0;
        
        // Use proper table macros for consistent formatting
        TABLE_HEADER_48("Thread Monitor", "Iteration Counts & Performance");
        
        // Log each thread's iteration count with error handling
        for (const auto& thread_info : thread_infos) {
            try {
                unsigned long iterations = thread_info.iterations.load();
                TABLE_ROW_48(thread_info.name, std::to_string(iterations) + " iterations");
            } catch (const std::exception& e) {
                // Skip invalid thread info and continue
                TABLE_ROW_48(thread_info.name, "ERROR: Invalid reference");
            }
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
    } catch (const std::exception& e) {
        // Log error and continue
        log_message("ERROR in thread monitoring stats: " + std::string(e.what()), "trading_system.log");
    } catch (...) {
        // Log unknown error and continue
        log_message("Unknown error in thread monitoring stats", "trading_system.log");
    }
}


void ThreadLogs::log_thread_status_table(const std::vector<AlpacaTrader::Config::ThreadStatusData>& thread_status_data) {
    TABLE_HEADER_48("Thread Configuration", "Priority    | CPU Affinity | Status");
    
    for (const auto& status : thread_status_data) {
        std::string status_text = status.success ? "OK" : "ERROR";
        std::string cpu_text = (status.cpu_core >= 0) ? "CPU " + std::to_string(status.cpu_core) : "None";
        
        // Format with proper spacing for alignment
        std::string priority_padded = status.priority;
        if (priority_padded.length() < 10) priority_padded += std::string(10 - priority_padded.length(), ' ');
        
        std::string cpu_padded = cpu_text;
        if (cpu_padded.length() < 13) cpu_padded += std::string(13 - cpu_padded.length(), ' ');
        
        std::string details = priority_padded + " | " + cpu_padded + " | " + status_text;
        
        TABLE_ROW_48(status.name, details);
    }
    
    TABLE_FOOTER_48();
}

// Thread registry error handling
void ThreadLogs::log_thread_registry_error(const std::string& error_msg) {
    LOG_THREAD_SECTION_HEADER("THREAD REGISTRY ERROR");
    LOG_THREAD_CONTENT("ERROR: " + error_msg);
    LOG_THREAD_SECTION_FOOTER();
    log_message("THREAD REGISTRY ERROR: " + error_msg, "trading_system.log");
}

void ThreadLogs::log_thread_registry_warning(const std::string& warning_msg) {
    LOG_THREAD_SECTION_HEADER("THREAD REGISTRY WARNING");
    LOG_THREAD_CONTENT("WARNING: " + warning_msg);
    LOG_THREAD_SECTION_FOOTER();
    log_message("THREAD REGISTRY WARNING: " + warning_msg, "trading_system.log");
}


// Thread exception handling
void ThreadLogs::log_thread_exception(const std::string& thread_name, const std::string& exception_msg) {
    LOG_THREAD_SECTION_HEADER("THREAD EXCEPTION");
    LOG_THREAD_CONTENT("THREAD: " + thread_name);
    LOG_THREAD_CONTENT("EXCEPTION: " + exception_msg);
    LOG_THREAD_SECTION_FOOTER();
    log_message(thread_name + " exception: " + exception_msg, "trading_system.log");
}

void ThreadLogs::log_thread_unknown_exception(const std::string& thread_name) {
    LOG_THREAD_SECTION_HEADER("THREAD UNKNOWN EXCEPTION");
    LOG_THREAD_CONTENT("THREAD: " + thread_name);
    LOG_THREAD_CONTENT("EXCEPTION: Unknown exception occurred");
    LOG_THREAD_SECTION_FOOTER();
    log_message(thread_name + " unknown exception", "trading_system.log");
}

// Thread configuration errors
void ThreadLogs::log_thread_config_error(const std::string& thread_name, const std::string& error_msg) {
    LOG_THREAD_SECTION_HEADER("THREAD CONFIG ERROR");
    LOG_THREAD_CONTENT("THREAD: " + thread_name);
    LOG_THREAD_CONTENT("ERROR: " + error_msg);
    LOG_THREAD_SECTION_FOOTER();
    log_message("THREAD CONFIG ERROR [" + thread_name + "]: " + error_msg, "trading_system.log");
}

void ThreadLogs::log_thread_config_warning(const std::string& thread_name, const std::string& warning_msg) {
    LOG_THREAD_SECTION_HEADER("THREAD CONFIG WARNING");
    LOG_THREAD_CONTENT("THREAD: " + thread_name);
    LOG_THREAD_CONTENT("WARNING: " + warning_msg);
    LOG_THREAD_SECTION_FOOTER();
    log_message("THREAD CONFIG WARNING [" + thread_name + "]: " + warning_msg, "trading_system.log");
}

// Thread lifecycle errors
void ThreadLogs::log_thread_startup_error(const std::string& thread_name, const std::string& error_msg) {
    LOG_THREAD_SECTION_HEADER("THREAD STARTUP ERROR");
    LOG_THREAD_CONTENT("THREAD: " + thread_name);
    LOG_THREAD_CONTENT("ERROR: " + error_msg);
    LOG_THREAD_SECTION_FOOTER();
    log_message("THREAD STARTUP ERROR [" + thread_name + "]: " + error_msg, "trading_system.log");
}

void ThreadLogs::log_thread_shutdown_error(const std::string& thread_name, const std::string& error_msg) {
    LOG_THREAD_SECTION_HEADER("THREAD SHUTDOWN ERROR");
    LOG_THREAD_CONTENT("THREAD: " + thread_name);
    LOG_THREAD_CONTENT("ERROR: " + error_msg);
    LOG_THREAD_SECTION_FOOTER();
    log_message("THREAD SHUTDOWN ERROR [" + thread_name + "]: " + error_msg, "trading_system.log");
}

// Centralized error message construction
std::string ThreadLogs::build_unknown_thread_type_error(const std::string& type_name, int enum_value) {
    return "CRITICAL ERROR: Unknown thread type requested: " + type_name + 
           " (enum value: " + std::to_string(enum_value) + ")";
}

