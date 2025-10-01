#ifndef SYSTEM_MONITOR_HPP
#define SYSTEM_MONITOR_HPP

#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>
#include "configs/strategy_config.hpp"

namespace AlpacaTrader {
namespace Core {
namespace Monitoring {

struct SystemMetrics {
    std::atomic<int> total_orders_placed{0};
    std::atomic<int> successful_orders{0};
    std::atomic<int> failed_orders{0};
    std::atomic<int> short_orders_blocked{0};
    std::atomic<int> data_freshness_failures{0};
    std::atomic<int> connectivity_issues{0};
    
    std::atomic<double> total_pnl{0.0};
    std::atomic<double> max_drawdown{0.0};
    std::atomic<double> current_drawdown{0.0};
    
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_successful_order;
    std::chrono::steady_clock::time_point last_data_update;
    
    SystemMetrics() : start_time(std::chrono::steady_clock::now()) {}
};

// Non-atomic version for return values
struct SystemMetricsSnapshot {
    int total_orders_placed{0};
    int successful_orders{0};
    int failed_orders{0};
    int short_orders_blocked{0};
    int data_freshness_failures{0};
    int connectivity_issues{0};
    
    double total_pnl{0.0};
    double max_drawdown{0.0};
    double current_drawdown{0.0};
    
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_successful_order;
    std::chrono::steady_clock::time_point last_data_update;
};

class SystemMonitor {
public:
    static SystemMonitor& instance();
    
    // Configuration
    void set_configuration(const StrategyConfig& config);
    
    // Metrics tracking
    void record_order_placed(bool success, const std::string& reason = "");
    void record_short_blocked(const std::string& symbol);
    void record_data_freshness_failure();
    void record_connectivity_issue();
    void update_pnl(double pnl);
    void update_drawdown(double current_drawdown);
    void record_data_update();
    
    // Health checks
    bool is_system_healthy() const;
    bool is_data_stale() const;
    bool has_recent_activity() const;
    
    // Reporting
    std::string get_health_report() const;
    std::string get_performance_summary() const;
    SystemMetricsSnapshot get_metrics() const;
    
    // Alerting
    void check_and_alert();
    
private:
    SystemMonitor() = default;
    mutable std::mutex metrics_mutex_;
    SystemMetrics metrics_;
    StrategyConfig config_;
    
    // Default alert thresholds (can be overridden by configuration)
    static constexpr int DEFAULT_MAX_FAILURE_RATE = 50; // 50% failure rate
    static constexpr int DEFAULT_MAX_STALE_DATA_MINUTES = 5;
    static constexpr int DEFAULT_MAX_INACTIVITY_MINUTES = 10;
    static constexpr double DEFAULT_MAX_DRAWDOWN_PCT = 10.0; // 10% max drawdown
};

} // namespace Monitoring
} // namespace Core
} // namespace AlpacaTrader

#endif // SYSTEM_MONITOR_HPP
