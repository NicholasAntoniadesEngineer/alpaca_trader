#include "risk_logs.hpp"
#include "trading_logs.hpp"
#include "async_logger.hpp"
#include <string>

namespace AlpacaTrader {
namespace Logging {

void RiskLogs::log_risk_assessment(const AlpacaTrader::Core::ProcessedData& data, double equity, bool allowed, const SystemConfig& config) {
    // Calculate daily PnL for logging
    double daily_pnl = 0.0; // Default for logging when initial equity is 0
    
    // Log risk conditions with calculated values
    TradingLogs::log_trading_conditions(daily_pnl, data.exposure_pct, allowed, config);
    
    // Log final risk status
    if (allowed) {
        log_risk_status(true);
    } else {
        log_risk_status(false, "Risk limits exceeded");
    }
}

void RiskLogs::log_risk_conditions(double daily_pnl, double exposure_pct, bool allowed, const AlpacaTrader::Core::ProcessedData& data, const SystemConfig& config) {
    TradingLogs::log_trading_conditions(daily_pnl, exposure_pct, allowed, config);
}

void RiskLogs::log_risk_status(bool allowed, const std::string& reason) {
    if (allowed) {
        TradingLogs::log_market_status(true);
    } else {
        TradingLogs::log_market_status(false, reason);
    }
}

} // namespace Logging
} // namespace AlpacaTrader
