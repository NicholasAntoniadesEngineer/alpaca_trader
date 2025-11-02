#ifndef STRATEGY_LOGIC_HPP
#define STRATEGY_LOGIC_HPP

#include "configs/system_config.hpp"
#include "trader/data_structures/data_structures.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

SignalDecision detect_trading_signals(const ProcessedData& processed_data_input, const SystemConfig& system_config);
FilterResult evaluate_trading_filters(const ProcessedData& processed_data_input, const SystemConfig& system_config);
PositionSizing calculate_position_sizing(const PositionSizingRequest& request);
ExitTargets compute_exit_targets(const ExitTargetsRequest& request);

// Strategy processing methods
std::pair<PositionSizing, SignalDecision> process_position_sizing(const PositionSizingProcessRequest& request);

} // namespace Core
} // namespace AlpacaTrader

#endif // STRATEGY_LOGIC_HPP


