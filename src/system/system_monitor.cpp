#include "system_monitor.hpp"
#include "logging/logs/system_logs.hpp"

namespace AlpacaTrader {
namespace Monitoring {

void SystemMonitor::set_configuration(const AlpacaTrader::Config::SystemConfig& config) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    config_ = config;
}

void SystemMonitor::record_startup_complete() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.startup_complete = true;
    metrics_.last_health_check_time = std::chrono::steady_clock::now();
    
    SystemLogs::log_startup_complete();
}

void SystemMonitor::record_configuration_validated(bool valid) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.configuration_valid = valid;
    
    if (!valid) {
        metrics_.critical_errors_count++;
    }
    
    SystemLogs::log_configuration_validated(valid);
}

void SystemMonitor::record_threads_started(int expected_thread_count, int actual_started_count) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.active_thread_count = actual_started_count;
    
    if (actual_started_count == expected_thread_count) {
        metrics_.all_threads_started = true;
    } else {
        metrics_.all_threads_started = false;
        metrics_.critical_errors_count++;
    }
    
    SystemLogs::log_threads_started(expected_thread_count, actual_started_count);
}

void SystemMonitor::record_thread_health_check(int active_thread_count) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.active_thread_count = active_thread_count;
    metrics_.last_health_check_time = std::chrono::steady_clock::now();
}

void SystemMonitor::record_connectivity_issue() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.connectivity_issues_count++;
    metrics_.last_connectivity_issue_time = std::chrono::steady_clock::now();
    
    SystemLogs::log_connectivity_issue();
}

void SystemMonitor::record_critical_error(const std::string& error_description) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.critical_errors_count++;
    
    SystemLogs::log_critical_error(error_description);
}

bool SystemMonitor::calculate_system_health_status() const {
    try {
    if (!metrics_.startup_complete) {
            return false;
    }
    
    if (!metrics_.configuration_valid) {
        return false;
    }
    
    if (!metrics_.all_threads_started) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_health_check = std::chrono::duration_cast<std::chrono::minutes>(now - metrics_.last_health_check_time);
    
    if (metrics_.last_health_check_time.time_since_epoch().count() > 0) {
            int max_health_check_interval_minutes_value = config_.timing.max_health_check_interval_minutes;
            if (time_since_health_check.count() > max_health_check_interval_minutes_value) {
        return false;
        }
    }
    
    return true;
    } catch (const std::exception& exception_error) {
        SystemLogs::log_critical_error(std::string("Error calculating system health status: ") + exception_error.what());
        return false;
    } catch (...) {
        SystemLogs::log_critical_error("Unknown error calculating system health status");
        return false;
    }
}

bool SystemMonitor::is_system_healthy() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return calculate_system_health_status();
}

bool SystemMonitor::are_threads_healthy(int expected_thread_count) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_.active_thread_count >= expected_thread_count;
}

bool SystemMonitor::is_configuration_valid() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_.configuration_valid;
}

bool SystemMonitor::has_startup_completed() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_.startup_complete;
}

std::string SystemMonitor::get_health_report() const {
    try {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
        bool system_healthy_value = calculate_system_health_status();
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - metrics_.startup_time);
        int uptime_seconds = static_cast<int>(uptime.count());
        
        return SystemLogs::format_health_report_string(
            system_healthy_value,
            metrics_.startup_complete,
            metrics_.configuration_valid,
            metrics_.all_threads_started,
            metrics_.active_thread_count.load(),
            metrics_.connectivity_issues_count.load(),
            metrics_.critical_errors_count.load(),
            uptime_seconds
        );
    } catch (const std::exception& exception_error) {
        SystemLogs::log_health_report_generation_error(exception_error.what());
        return SystemLogs::format_health_report_error_string();
    } catch (...) {
        SystemLogs::log_health_report_generation_error("Unknown error");
        return SystemLogs::format_health_report_error_string();
    }
}

SystemHealthSnapshot SystemMonitor::get_health_snapshot() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    SystemHealthSnapshot result;
    result.startup_complete = metrics_.startup_complete;
    result.configuration_valid = metrics_.configuration_valid;
    result.all_threads_started = metrics_.all_threads_started;
    result.active_thread_count = metrics_.active_thread_count;
    result.connectivity_issues_count = metrics_.connectivity_issues_count;
    result.critical_errors_count = metrics_.critical_errors_count;
    result.startup_time = metrics_.startup_time;
    result.last_health_check_time = metrics_.last_health_check_time;
    result.last_connectivity_issue_time = metrics_.last_connectivity_issue_time;
    return result;
}

void SystemMonitor::log_health_report() const {
    try {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
        // Calculate overall health status using shared helper
        bool system_healthy_value = calculate_system_health_status();
    
    // Calculate uptime
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - metrics_.startup_time);
    int uptime_seconds = static_cast<int>(uptime.count());
    
    // Use SystemLogs to format and log the table
    SystemLogs::log_health_report_table(
        system_healthy_value,
        metrics_.startup_complete,
        metrics_.configuration_valid,
        metrics_.all_threads_started,
        metrics_.active_thread_count.load(),
        metrics_.connectivity_issues_count.load(),
        metrics_.critical_errors_count.load(),
        uptime_seconds
    );
    } catch (const std::exception& exception_error) {
        SystemLogs::log_critical_error(std::string("Error logging health report: ") + exception_error.what());
    } catch (...) {
        SystemLogs::log_critical_error("Unknown error logging health report");
    }
}

void SystemMonitor::check_and_alert() {
    if (!is_system_healthy()) {
        SystemLogs::log_system_alert(get_health_report());
    }
}


} // namespace Monitoring
} // namespace AlpacaTrader
