#include "risk_manager.hpp"
#include "core/logging/logger/async_logger.hpp"
#include <cmath>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::RiskLogs;

RiskManager::RiskManager(const SystemConfig& cfg) : config(cfg) {}

bool RiskManager::validate_trading_permissions(const ProcessedData& data, double equity) {
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
    if (data.exposure_pct > config.strategy.max_account_exposure_percentage) {
        return false;
    }
    
    // Check absolute exposure amount limit (if configured)
    double max_exposure_amount = equity * config.strategy.max_account_exposure_percentage / 100.0;
    double current_exposure_amount = equity * data.exposure_pct / 100.0;
    
    // Additional validation: ensure we don't exceed maximum exposure amount
    return current_exposure_amount <= max_exposure_amount;
}

bool RiskManager::check_daily_limits(double current_equity, double initial_equity) {
    if (initial_equity == 0.0) {
        return true;
    }
    
    double daily_pnl = (current_equity - initial_equity) / initial_equity;
    return daily_pnl > config.strategy.max_daily_loss_percentage && daily_pnl < config.strategy.daily_profit_target_percentage;
}


RiskManager::TradeGateInput RiskManager::build_risk_input(const ProcessedData& data, double equity) {
    TradeGateInput input;
    input.initial_equity = 0.0;
    input.current_equity = equity;
    input.exposure_pct = data.exposure_pct;
    return input;
}

bool RiskManager::evaluate_risk_gate(const TradeGateInput& input) {
    TradeGateResult result = evaluate_trade_gate(input);
    return result.pnl_ok && result.exposure_ok;
}

RiskManager::TradeGateResult RiskManager::evaluate_trade_gate(const TradeGateInput& input) {
    TradeGateResult result;
    result.daily_pnl = (input.initial_equity == 0.0) ? 0.0 : (input.current_equity - input.initial_equity) / input.initial_equity;
    result.pnl_ok = result.daily_pnl > config.strategy.max_daily_loss_percentage && result.daily_pnl < config.strategy.daily_profit_target_percentage;
    result.exposure_ok = input.exposure_pct <= config.strategy.max_account_exposure_percentage;
    result.allowed = result.pnl_ok && result.exposure_ok;
    return result;
}

double RiskManager::calculate_exposure_percentage(double current_value, double equity) {
    if (equity <= 0.0) {
        return 0.0;
    }
    return (std::abs(current_value) / equity) * 100.0;
}

} // namespace Core
} // namespace AlpacaTrader
