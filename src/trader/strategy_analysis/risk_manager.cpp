#include "risk_manager.hpp"
#include <cmath>
#include <stdexcept>

namespace AlpacaTrader {
    namespace Core {

RiskManager::RiskManager(const SystemConfig& system_config) : config(system_config) {}

bool RiskManager::validate_trading_permissions(const ProcessedData& data, double current_equity, double initial_equity) {
    
    if (initial_equity <= 0.0 || !std::isfinite(initial_equity)) {
        throw std::runtime_error("Invalid initial equity for trading permissions validation: " + std::to_string(initial_equity));
    }
    
    
    bool daily_limits_ok = check_daily_limits(current_equity, initial_equity);
    
    
    if (!daily_limits_ok) {
        return false;
    }
    
    
    bool exposure_limits_ok = check_exposure_limits(data, current_equity);
    
    
    if (!exposure_limits_ok) {
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
    double max_exposure_amount = equity * config.strategy.max_account_exposure_percentage / config.strategy.percentage_calculation_multiplier;
    double current_exposure_amount = equity * data.exposure_pct / config.strategy.percentage_calculation_multiplier;
    
    
    // Additional validation: ensure we don't exceed maximum exposure amount
    bool result = current_exposure_amount <= max_exposure_amount;
    
    
    return result;
}

bool RiskManager::check_daily_limits(double current_equity, double initial_equity) {
    
    if (initial_equity <= 0.0 || !std::isfinite(initial_equity)) {
        throw std::runtime_error("Invalid initial equity for daily limits check: " + std::to_string(initial_equity));
    }
    
    if (current_equity < 0.0 || !std::isfinite(current_equity)) {
        throw std::runtime_error("Invalid current equity for daily limits check: " + std::to_string(current_equity));
    }
    
    double daily_pnl = (current_equity - initial_equity) / initial_equity;
    
    
    bool result = daily_pnl > config.strategy.max_daily_loss_percentage && daily_pnl < config.strategy.daily_profit_target_percentage;
    
    
    return result;
}


RiskManager::TradeGateInput RiskManager::build_risk_input(const ProcessedData& data, double current_equity, double initial_equity) {
    if (initial_equity <= 0.0 || !std::isfinite(initial_equity)) {
        throw std::runtime_error("Invalid initial equity for risk input: " + std::to_string(initial_equity));
    }
    
    if (current_equity < 0.0 || !std::isfinite(current_equity)) {
        throw std::runtime_error("Invalid current equity for risk input: " + std::to_string(current_equity));
    }
    
    TradeGateInput input;
    input.initial_equity = initial_equity;
    input.current_equity = current_equity;
    input.exposure_pct = data.exposure_pct;
    return input;
}

bool RiskManager::evaluate_risk_gate(const TradeGateInput& input) {
    TradeGateResult result = evaluate_trade_gate(input);
    return result.pnl_ok && result.exposure_ok;
}

RiskManager::TradeGateResult RiskManager::evaluate_trade_gate(const TradeGateInput& input) {
    if (input.initial_equity <= 0.0 || !std::isfinite(input.initial_equity)) {
        throw std::runtime_error("Invalid initial equity in trade gate evaluation: " + std::to_string(input.initial_equity));
    }
    
    if (input.current_equity < 0.0 || !std::isfinite(input.current_equity)) {
        throw std::runtime_error("Invalid current equity in trade gate evaluation: " + std::to_string(input.current_equity));
    }
    
    TradeGateResult result;
    result.daily_pnl = (input.current_equity - input.initial_equity) / input.initial_equity;
    result.pnl_ok = result.daily_pnl > config.strategy.max_daily_loss_percentage && result.daily_pnl < config.strategy.daily_profit_target_percentage;
    result.exposure_ok = input.exposure_pct <= config.strategy.max_account_exposure_percentage;
    result.allowed = result.pnl_ok && result.exposure_ok;
    return result;
}

double RiskManager::calculate_exposure_percentage(double current_value, double equity) {
    if (equity <= 0.0 || !std::isfinite(equity)) {
        throw std::runtime_error("Invalid equity for exposure percentage calculation: " + std::to_string(equity));
    }
    if (!std::isfinite(current_value)) {
        throw std::runtime_error("Invalid current value for exposure percentage calculation: " + std::to_string(current_value));
    }
    return (std::abs(current_value) / equity) * config.strategy.percentage_calculation_multiplier;
}

} // namespace Core
} // namespace AlpacaTrader
