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

class SystemMonitor {
public:
    SystemMonitor() = default;
    
    // Configuration
    void set_configuration(const AlpacaTrader::Config::SystemConfig& config);
    
    // Startup validation
    void record_startup_complete();
    void record_configuration_validated(bool valid);
    void record_threads_started(int expected_thread_count, int actual_started_count);
    
    // Runtime health monitoring
    void record_thread_health_check(int active_thread_count);
    void record_connectivity_issue();
    void record_critical_error(const std::string& error_description);
    
    // Health checks
    bool is_system_healthy() const;
    bool are_threads_healthy(int expected_thread_count) const;
    bool is_configuration_valid() const;
    bool has_startup_completed() const;
    
    // Reporting
    std::string get_health_report() const;
    SystemHealthSnapshot get_health_snapshot() const;
    void log_health_report() const;
    
    // Alerting
    void check_and_alert();
    
private:
    mutable std::mutex metrics_mutex_;
    SystemHealthMetrics metrics_;
    AlpacaTrader::Config::SystemConfig config_;
    
    bool calculate_system_health_status() const;
};

} // namespace Monitoring
} // namespace AlpacaTrader

#endif // SYSTEM_MONITOR_HPP
