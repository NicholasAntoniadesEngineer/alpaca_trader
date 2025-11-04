#ifndef SYSTEM_MONITOR_HPP
#define SYSTEM_MONITOR_HPP

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include "configs/system_config.hpp"

namespace AlpacaTrader {
namespace Monitoring {

struct SystemHealthMetrics {
    std::atomic<bool> startup_complete{false};
    std::atomic<bool> configuration_valid{false};
    std::atomic<bool> all_threads_started{false};
    std::atomic<int> active_thread_count{0};
    std::atomic<int> connectivity_issues_count{0};
    std::atomic<int> critical_errors_count{0};
    
    std::chrono::steady_clock::time_point startup_time;
    std::chrono::steady_clock::time_point last_health_check_time;
    std::chrono::steady_clock::time_point last_connectivity_issue_time;
    
    SystemHealthMetrics() : startup_time(std::chrono::steady_clock::now()) {}
};

struct SystemHealthSnapshot {
    bool startup_complete{false};
    bool configuration_valid{false};
    bool all_threads_started{false};
    int active_thread_count{0};
    int connectivity_issues_count{0};
    int critical_errors_count{0};
    
    std::chrono::steady_clock::time_point startup_time;
    std::chrono::steady_clock::time_point last_health_check_time;
    std::chrono::steady_clock::time_point last_connectivity_issue_time;
};

struct SystemHealthReport {
    bool system_healthy_value{false};
    bool startup_complete_value{false};
    bool configuration_valid_value{false};
    bool all_threads_started_value{false};
    int active_thread_count_value{0};
    int connectivity_issues_count_value{0};
    int critical_errors_count_value{0};
    int uptime_seconds_value{0};
};

class SystemMonitor {
public:
    SystemMonitor() = default;
    
    // Configuration
    bool set_configuration(const AlpacaTrader::Config::SystemConfig& config);
    
    // Startup validation
    bool record_startup_complete();
    bool record_configuration_validated(bool valid);
    bool record_threads_started(int expected_thread_count, int actual_started_count);
    
    // Runtime health monitoring
    bool record_thread_health_check(int active_thread_count);
    bool record_connectivity_issue();
    bool record_critical_error(const std::string&);
    
    // Health checks
    bool is_system_healthy() const;
    bool are_threads_healthy(int expected_thread_count) const;
    bool is_configuration_valid() const;
    bool has_startup_completed() const;
    
    // Reporting
    std::string get_health_report() const;
    SystemHealthSnapshot get_health_snapshot() const;
    SystemHealthReport get_health_report_data() const;
    
    // Alerting
    bool should_alert() const;
    
private:
    mutable std::mutex metrics_mutex_;
    SystemHealthMetrics metrics_;
    AlpacaTrader::Config::SystemConfig config_;
    
    bool calculate_system_health_status() const;
};

} // namespace Monitoring
} // namespace AlpacaTrader

#endif // SYSTEM_MONITOR_HPP
