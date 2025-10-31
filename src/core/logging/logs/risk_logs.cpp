#include "risk_logs.hpp"
#include "trading_logs.hpp"
#include "core/logging/logger/async_logger.hpp"
#include <string>
#include <cmath>

namespace AlpacaTrader {
namespace Logging {

void RiskLogs::log_risk_assessment(const AlpacaTrader::Core::ProcessedData& data, bool allowed, const SystemConfig& config, double current_equity, double initial_equity) {
    if (initial_equity <= 0.0 || !std::isfinite(initial_equity)) {
        TradingLogs::log_market_status(false, "Invalid initial equity for risk assessment - must be positive and finite");
        throw std::runtime_error("Invalid initial equity for risk assessment: " + std::to_string(initial_equity));
    }
    
    if (current_equity < 0.0 || !std::isfinite(current_equity)) {
        TradingLogs::log_market_status(false, "Invalid current equity for risk assessment - must be non-negative and finite");
        throw std::runtime_error("Invalid current equity for risk assessment: " + std::to_string(current_equity));
    }
    
    double daily_pnl = (current_equity - initial_equity) / initial_equity;
    
    TradingLogs::log_trading_conditions(daily_pnl, data.exposure_pct, allowed, config);
    
    if (allowed) {
        log_risk_status(true, "Risk assessment passed");
    } else {
        log_risk_status(false, "Risk limits exceeded");
    }
}

void RiskLogs::log_risk_conditions(double daily_pnl, double exposure_pct, bool allowed, const SystemConfig& config) {
    TradingLogs::log_trading_conditions(daily_pnl, exposure_pct, allowed, config);
}

void RiskLogs::log_risk_status(bool allowed, const std::string& reason) {
    if (allowed) {
        TradingLogs::log_market_status(true, "Risk assessment passed - trading allowed");
    } else {
        TradingLogs::log_market_status(false, reason);
    }
}

} // namespace Logging
} // namespace AlpacaTrader
