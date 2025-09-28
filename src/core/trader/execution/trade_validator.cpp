#include "trade_validator.hpp"
#include "core/logging/trading_logs.hpp"
#include <sstream>
#include <iomanip>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

TradeValidator::TradeValidator(const TraderConfig& cfg) : config(cfg) {}

bool TradeValidator::validate_trade_feasibility(const StrategyLogic::PositionSizing& sizing, double buying_power, double current_price) const {
    if (sizing.quantity <= 0) {
        return false;
    }
    
    double position_value = sizing.quantity * current_price;
    double required_buying_power = position_value * config.risk.buying_power_validation_factor;
    
    if (buying_power < required_buying_power) {
        std::ostringstream oss;
        oss << "Insufficient buying power: Need $" << std::fixed << std::setprecision(2) << required_buying_power 
            << ", Have $" << std::fixed << std::setprecision(2) << buying_power 
            << " (Position: " << sizing.quantity << " @ $" << std::fixed << std::setprecision(2) << current_price << ")";
        TradingLogs::log_trade_validation_failed(oss.str());
        return false;
    }
    
    return true;
}

} // namespace Core
} // namespace AlpacaTrader
