#ifndef TRADE_VALIDATOR_HPP
#define TRADE_VALIDATOR_HPP

#include "configs/trader_config.hpp"
#include "../analysis/strategy_logic.hpp"
#include "core/logging/trading_logs.hpp"
#include <sstream>
#include <iomanip>

namespace AlpacaTrader {
namespace Core {

class TradeValidator {
public:
    TradeValidator(const TraderConfig& config);
    
    bool validate_trade_feasibility(const StrategyLogic::PositionSizing& sizing, double buying_power, double current_price) const;
    
private:
    const TraderConfig& config;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADE_VALIDATOR_HPP
