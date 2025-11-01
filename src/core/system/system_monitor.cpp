#include "system_monitor.hpp"
#include "core/logging/logger/async_logger.hpp"
#include <sstream>
#include <iomanip>

namespace AlpacaTrader {
namespace Core {
namespace Monitoring {

void SystemMonitor::set_configuration(const StrategyConfig& config) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    config_ = config;
}

void SystemMonitor::record_order_placed(bool success, const std::string& reason) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    metrics_.total_orders_placed++;
    
    if (success) {
        metrics_.successful_orders++;
        metrics_.last_successful_order = std::chrono::steady_clock::now();
    } else {
        metrics_.failed_orders++;
        
        // Log specific failure reasons
        if (!reason.empty()) {
            AlpacaTrader::Logging::log_message("ORDER_FAILURE: " + reason, "system_monitor.log");
        }
    }
}

void SystemMonitor::record_short_blocked(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.short_orders_blocked++;
    
    AlpacaTrader::Logging::log_message("SHORT_BLOCKED: " + symbol + " - insufficient short availability", "system_monitor.log");
}

void SystemMonitor::record_data_freshness_failure() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.data_freshness_failures++;
    
    AlpacaTrader::Logging::log_message("DATA_STALE: Market data is stale - trading halted", "system_monitor.log");
}

void SystemMonitor::record_connectivity_issue() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.connectivity_issues++;
    
    AlpacaTrader::Logging::log_message("CONNECTIVITY_ISSUE: API connectivity problem detected", "system_monitor.log");
}

void SystemMonitor::update_pnl(double pnl) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.total_pnl = pnl;
    
    // Update drawdown if P&L is negative
    if (pnl < 0) {
        double drawdown_pct = std::abs(pnl) * 100.0;
        if (drawdown_pct > metrics_.current_drawdown) {
            metrics_.current_drawdown = drawdown_pct;
            if (drawdown_pct > metrics_.max_drawdown) {
                metrics_.max_drawdown = drawdown_pct;
            }
        }
    } else {
        metrics_.current_drawdown = 0.0;
    }
}

void SystemMonitor::update_drawdown(double current_drawdown) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.current_drawdown = current_drawdown;
    
    if (current_drawdown > metrics_.max_drawdown) {
        metrics_.max_drawdown = current_drawdown;
    }
}

void SystemMonitor::record_data_update() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.last_data_update = std::chrono::steady_clock::now();
}

bool SystemMonitor::is_system_healthy() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    if (config_.max_failure_rate_pct <= 0.0) {
        throw std::runtime_error("System monitoring: max_failure_rate_pct must be configured (no defaults allowed)");
    }
    if (config_.max_drawdown_pct <= 0.0) {
        throw std::runtime_error("System monitoring: max_drawdown_pct must be configured (no defaults allowed)");
    }
    
    double max_failure_rate = config_.max_failure_rate_pct;
    double max_drawdown = config_.max_drawdown_pct;
    
    // Check failure rate
    if (metrics_.total_orders_placed > 10) {
        double failure_rate = (static_cast<double>(metrics_.failed_orders) / metrics_.total_orders_placed) * 100.0;
        if (failure_rate > max_failure_rate) {
            return false;
        }
    }
    
    // Check data freshness
    if (is_data_stale()) {
        return false;
    }
    
    // Check recent activity
    if (!has_recent_activity()) {
        return false;
    }
    
    // Check drawdown
    if (metrics_.current_drawdown > max_drawdown) {
        return false;
    }
    
    return true;
}

bool SystemMonitor::is_data_stale() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_update = std::chrono::duration_cast<std::chrono::minutes>(now - metrics_.last_data_update);
    
    if (config_.max_data_age_min <= 0) {
        throw std::runtime_error("System monitoring: max_data_age_min must be configured (no defaults allowed)");
    }
    
    int max_stale_minutes = config_.max_data_age_min;
    
    return time_since_update.count() > max_stale_minutes;
}

bool SystemMonitor::has_recent_activity() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_activity = std::chrono::duration_cast<std::chrono::minutes>(now - metrics_.last_successful_order);
    
    if (config_.max_inactivity_min <= 0) {
        throw std::runtime_error("System monitoring: max_inactivity_min must be configured (no defaults allowed)");
    }
    
    int max_inactivity_minutes = config_.max_inactivity_min;
    
    return time_since_activity.count() < max_inactivity_minutes;
}

std::string SystemMonitor::get_health_report() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::ostringstream oss;
    oss << "=== SYSTEM HEALTH REPORT ===\n";
    oss << "Overall Health: " << (is_system_healthy() ? "HEALTHY" : "UNHEALTHY") << "\n";
    oss << "Data Freshness: " << (is_data_stale() ? "STALE" : "FRESH") << "\n";
    oss << "Recent Activity: " << (has_recent_activity() ? "ACTIVE" : "INACTIVE") << "\n";
    oss << "Current Drawdown: " << std::fixed << std::setprecision(2) << metrics_.current_drawdown << "%\n";
    oss << "Max Drawdown: " << std::fixed << std::setprecision(2) << metrics_.max_drawdown << "%\n";
    
    return oss.str();
}

std::string SystemMonitor::get_performance_summary() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::ostringstream oss;
    oss << "=== PERFORMANCE SUMMARY ===\n";
    oss << "Total Orders: " << metrics_.total_orders_placed << "\n";
    oss << "Successful: " << metrics_.successful_orders << "\n";
    oss << "Failed: " << metrics_.failed_orders << "\n";
    oss << "Short Blocked: " << metrics_.short_orders_blocked << "\n";
    oss << "Data Failures: " << metrics_.data_freshness_failures << "\n";
    oss << "Connectivity Issues: " << metrics_.connectivity_issues << "\n";
    
    if (metrics_.total_orders_placed > 0) {
        double success_rate = (static_cast<double>(metrics_.successful_orders) / metrics_.total_orders_placed) * 100.0;
        oss << "Success Rate: " << std::fixed << std::setprecision(2) << success_rate << "%\n";
    }
    
    oss << "Total P&L: $" << std::fixed << std::setprecision(2) << metrics_.total_pnl << "\n";
    
    return oss.str();
}

SystemMetricsSnapshot SystemMonitor::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    SystemMetricsSnapshot result;
    result.total_orders_placed = metrics_.total_orders_placed.load();
    result.successful_orders = metrics_.successful_orders.load();
    result.failed_orders = metrics_.failed_orders.load();
    result.short_orders_blocked = metrics_.short_orders_blocked.load();
    result.data_freshness_failures = metrics_.data_freshness_failures.load();
    result.connectivity_issues = metrics_.connectivity_issues.load();
    result.total_pnl = metrics_.total_pnl.load();
    result.max_drawdown = metrics_.max_drawdown.load();
    result.current_drawdown = metrics_.current_drawdown.load();
    result.start_time = metrics_.start_time;
    result.last_successful_order = metrics_.last_successful_order;
    result.last_data_update = metrics_.last_data_update;
    return result;
}

void SystemMonitor::check_and_alert() {
    if (!is_system_healthy()) {
        std::string alert = "SYSTEM ALERT: " + get_health_report();
        AlpacaTrader::Logging::log_message(alert, "system_alerts.log");
        
        // In production, you would send alerts via email, SMS, or webhook
        // For now, we just log to file
    }
}

} // namespace Monitoring
} // namespace Core
} // namespace AlpacaTrader
