#include "system_monitor.hpp"
#include "logging/logs/system_logs.hpp"
#include <sstream>
#include <iomanip>

namespace AlpacaTrader {
namespace Core {
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

bool SystemMonitor::is_system_healthy() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
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
        int max_health_check_interval_minutes = 10;
        if (time_since_health_check.count() > max_health_check_interval_minutes) {
        return false;
        }
    }
    
    return true;
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
    
    std::ostringstream oss;
    oss << "=== SYSTEM HEALTH REPORT ===\n";
    oss << "Overall Health: " << (is_system_healthy() ? "HEALTHY" : "UNHEALTHY") << "\n";
    oss << "Startup Complete: " << (metrics_.startup_complete ? "YES" : "NO") << "\n";
    oss << "Configuration Valid: " << (metrics_.configuration_valid ? "YES" : "NO") << "\n";
    oss << "All Threads Started: " << (metrics_.all_threads_started ? "YES" : "NO") << "\n";
    oss << "Active Threads: " << metrics_.active_thread_count.load() << "\n";
    oss << "Connectivity Issues: " << metrics_.connectivity_issues_count.load() << "\n";
    oss << "Critical Errors: " << metrics_.critical_errors_count.load() << "\n";
    
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - metrics_.startup_time);
    oss << "System Uptime: " << uptime.count() << " seconds\n";
    
    return oss.str();
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

void SystemMonitor::check_and_alert() {
    if (!is_system_healthy()) {
        SystemLogs::log_system_alert(get_health_report());
    }
}

bool SystemMonitor::validate_config_loaded() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return !config_.trading_mode.primary_symbol.empty();
}

} // namespace Monitoring
} // namespace Core
} // namespace AlpacaTrader
