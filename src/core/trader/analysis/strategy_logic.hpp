#ifndef STRATEGY_LOGIC_HPP
#define STRATEGY_LOGIC_HPP

#include "configs/system_config.hpp"
#include "core/trader/data/data_structures.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

SignalDecision detect_trading_signals(const ProcessedData& data, const SystemConfig& config);
FilterResult evaluate_trading_filters(const ProcessedData& data, const SystemConfig& config);
PositionSizing calculate_position_sizing(const PositionSizingRequest& request);
ExitTargets compute_exit_targets(const ExitTargetsRequest& request);

// Strategy processing methods
void process_signal_analysis(const ProcessedData& data, const SystemConfig& config);
std::pair<PositionSizing, SignalDecision> process_position_sizing(const PositionSizingProcessRequest& request);

} // namespace Core
} // namespace AlpacaTrader

#endif // STRATEGY_LOGIC_HPP


