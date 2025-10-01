#ifndef STRATEGY_LOGIC_HPP
#define STRATEGY_LOGIC_HPP

#include "configs/trader_config.hpp"
#include "../data/data_structures.hpp"

namespace AlpacaTrader {
namespace Core {

namespace StrategyLogic {

SignalDecision detect_trading_signals(const ProcessedData& data, const TraderConfig& config);
FilterResult evaluate_trading_filters(const ProcessedData& data, const TraderConfig& config);
PositionSizing calculate_position_sizing(const ProcessedData& data, double equity, int current_qty, const TraderConfig& config, double buying_power = 0.0);
ExitTargets compute_exit_targets(const std::string& side, double entry_price, double risk_amount, double rr_ratio, const TraderConfig& config);

} // namespace StrategyLogic
} // namespace Core
} // namespace AlpacaTrader

#endif // STRATEGY_LOGIC_HPP


