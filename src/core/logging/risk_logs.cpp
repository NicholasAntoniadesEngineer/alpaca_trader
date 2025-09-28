#include "risk_logs.hpp"
#include "trading_logs.hpp"
#include "async_logger.hpp"
#include <string>

namespace AlpacaTrader {
namespace Logging {

void RiskLogs::log_risk_assessment(const AlpacaTrader::Core::ProcessedData& data, double equity, bool allowed, const TraderConfig& config) {
    // Build risk input for evaluation
    AlpacaTrader::Core::RiskLogic::TradeGateInput input;
    input.initial_equity = 0.0;
    input.current_equity = equity;
    input.exposure_pct = data.exposure_pct;
    
    // Evaluate risk gate
    AlpacaTrader::Core::RiskLogic::TradeGateResult result = AlpacaTrader::Core::RiskLogic::evaluate_trade_gate(input, config);
    
    // Log risk conditions
    log_risk_conditions(result, data, config);
    
    // Log final risk status
    if (allowed) {
        log_risk_status(true);
    } else {
        log_risk_status(false, "Risk limits exceeded");
    }
}

void RiskLogs::log_risk_conditions(const AlpacaTrader::Core::RiskLogic::TradeGateResult& result, const AlpacaTrader::Core::ProcessedData& data, const TraderConfig& config) {
    TradingLogs::log_trading_conditions(result.daily_pnl, data.exposure_pct, result.allowed, config);
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
