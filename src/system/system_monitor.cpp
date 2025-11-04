#include "system_monitor.hpp"

namespace AlpacaTrader {
namespace Monitoring {

bool SystemMonitor::set_configuration(const AlpacaTrader::Config::SystemConfig& config) {
    try {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        config_ = config;
        return true;
    } catch (const std::exception&) {
        return false;
    } catch (...) {
        return false;
    }
}

bool SystemMonitor::record_startup_complete() {
    try {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.startup_complete = true;
        metrics_.last_health_check_time = std::chrono::steady_clock::now();
        return true;
    } catch (const std::exception&) {
        return false;
    } catch (...) {
        return false;
    }
}

bool SystemMonitor::record_configuration_validated(bool valid) {
    try {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.configuration_valid = valid;
        
        if (!valid) {
            metrics_.critical_errors_count++;
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    } catch (...) {
        return false;
    }
}

bool SystemMonitor::record_threads_started(int expected_thread_count, int actual_started_count) {
    try {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.active_thread_count = actual_started_count;
        
        if (actual_started_count == expected_thread_count) {
            metrics_.all_threads_started = true;
        } else {
            metrics_.all_threads_started = false;
            metrics_.critical_errors_count++;
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    } catch (...) {
        return false;
    }
}

bool SystemMonitor::record_thread_health_check(int active_thread_count) {
    try {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.active_thread_count = active_thread_count;
        metrics_.last_health_check_time = std::chrono::steady_clock::now();
        return true;
    } catch (const std::exception&) {
        return false;
    } catch (...) {
        return false;
    }
}

bool SystemMonitor::record_connectivity_issue() {
    try {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.connectivity_issues_count++;
        metrics_.last_connectivity_issue_time = std::chrono::steady_clock::now();
        return true;
    } catch (const std::exception&) {
        return false;
    } catch (...) {
        return false;
    }
}

bool SystemMonitor::record_critical_error(const std::string&) {
    try {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.critical_errors_count++;
        return true;
    } catch (const std::exception&) {
        return false;
    } catch (...) {
        return false;
    }
}

bool SystemMonitor::calculate_system_health_status() const {
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
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    SystemHealthReport report_data = get_health_report_data();
    
    // Format basic string representation without logging dependencies
    std::string health_status_string = report_data.system_healthy_value ? "HEALTHY" : "UNHEALTHY";
    std::string report_string = "System Health: " + health_status_string + 
                                ", Uptime: " + std::to_string(report_data.uptime_seconds_value) + " seconds";
    
    return report_string;
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

SystemHealthReport SystemMonitor::get_health_report_data() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    SystemHealthReport report_result;
    report_result.system_healthy_value = calculate_system_health_status();
    report_result.startup_complete_value = metrics_.startup_complete;
    report_result.configuration_valid_value = metrics_.configuration_valid;
    report_result.all_threads_started_value = metrics_.all_threads_started;
    report_result.active_thread_count_value = metrics_.active_thread_count.load();
    report_result.connectivity_issues_count_value = metrics_.connectivity_issues_count.load();
    report_result.critical_errors_count_value = metrics_.critical_errors_count.load();
    
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - metrics_.startup_time);
    report_result.uptime_seconds_value = static_cast<int>(uptime.count());
    
    return report_result;
}

bool SystemMonitor::should_alert() const {
    return !is_system_healthy();
}


} // namespace Monitoring
} // namespace AlpacaTrader
