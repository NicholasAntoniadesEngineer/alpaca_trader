#include "risk_manager.hpp"
#include "core/utils/connectivity_manager.hpp"
#include "core/logging/async_logger.hpp"

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

RiskManager::RiskManager(const TraderConfig& cfg) : config(cfg) {}

bool RiskManager::validate_trading_permissions(const ProcessedData& data, double equity) {
    if (!check_connectivity_status()) {
        return false;
    }
    
    if (!check_daily_limits(equity, 0.0)) {
        return false;
    }
    
    if (!check_exposure_limits(data, equity)) {
        return false;
    }
    
    return true;
}

bool RiskManager::check_exposure_limits(const ProcessedData& data, double equity) {
    // Check percentage-based exposure limit
    if (data.exposure_pct > config.risk.max_exposure_pct) {
        return false;
    }
    
    // Check absolute exposure amount limit (if configured)
    double max_exposure_amount = equity * config.risk.max_exposure_pct / 100.0;
    double current_exposure_amount = equity * data.exposure_pct / 100.0;
    
    // Additional validation: ensure we don't exceed maximum exposure amount
    return current_exposure_amount <= max_exposure_amount;
}

bool RiskManager::check_daily_limits(double current_equity, double initial_equity) {
    if (initial_equity == 0.0) {
        return true;
    }
    
    double daily_pnl = (current_equity - initial_equity) / initial_equity;
    return daily_pnl > config.risk.daily_max_loss && daily_pnl < config.risk.daily_profit_target;
}

bool RiskManager::check_connectivity_status() {
    auto& connectivity = ConnectivityManager::instance();
    if (connectivity.is_connectivity_outage()) {
        std::string connectivity_msg = "Connectivity outage - status: " + connectivity.get_status_string();
        TradingLogs::log_market_status(false, connectivity_msg);
        return false;
    }
    return true;
}

void RiskManager::log_risk_assessment(const ProcessedData& data, double equity, bool allowed) {
    RiskLogic::TradeGateInput input = build_risk_input(data, equity);
    RiskLogic::TradeGateResult result = RiskLogic::evaluate_trade_gate(input, config);
    
    log_risk_conditions(result, data);
    
    if (allowed) {
        TradingLogs::log_market_status(true);
    } else {
        TradingLogs::log_market_status(false, "Risk limits exceeded");
    }
}

RiskLogic::TradeGateInput RiskManager::build_risk_input(const ProcessedData& data, double equity) {
    RiskLogic::TradeGateInput input;
    input.initial_equity = 0.0;
    input.current_equity = equity;
    input.exposure_pct = data.exposure_pct;
    return input;
}

bool RiskManager::evaluate_risk_gate(const RiskLogic::TradeGateInput& input) {
    RiskLogic::TradeGateResult result = RiskLogic::evaluate_trade_gate(input, config);
    return result.pnl_ok && result.exposure_ok;
}

void RiskManager::log_risk_conditions(const RiskLogic::TradeGateResult& result, const ProcessedData& data) {
    TradingLogs::log_trading_conditions(result.daily_pnl, data.exposure_pct, result.allowed, config);
}

} // namespace Core
} // namespace AlpacaTrader
