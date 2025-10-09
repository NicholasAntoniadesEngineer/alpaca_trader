#include "trade_validator.hpp"
#include "core/logging/trading_logs.hpp"

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

TradeValidator::TradeValidator(const SystemConfig& cfg) : config(cfg) {}

bool TradeValidator::validate_trade_feasibility(const StrategyLogic::PositionSizing& sizing, double buying_power, double current_price) const {
    if (sizing.quantity <= 0) {
        return false;
    }
    
    double position_value = sizing.quantity * current_price;
    double required_buying_power = position_value * config.strategy.buying_power_validation_safety_margin;
    
    if (buying_power < required_buying_power) {
        TradingLogs::log_insufficient_buying_power(required_buying_power, buying_power, sizing.quantity, current_price);
        return false;
    }
    
    return true;
}

} // namespace Core
} // namespace AlpacaTrader
