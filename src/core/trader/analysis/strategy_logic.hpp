#ifndef STRATEGY_LOGIC_HPP
#define STRATEGY_LOGIC_HPP

#include "configs/system_config.hpp"
#include "core/trader/data/data_structures.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

SignalDecision detect_trading_signals(const ProcessedData& data, const SystemConfig& config);
FilterResult evaluate_trading_filters(const ProcessedData& data, const SystemConfig& config);
PositionSizing calculate_position_sizing(const ProcessedData& data, double equity, int current_qty, const SystemConfig& config, double buying_power = 0.0);
ExitTargets compute_exit_targets(const std::string& side, double entry_price, double risk_amount, double rr_ratio, const SystemConfig& config);

// Strategy processing methods
void process_signal_analysis(const ProcessedData& data, const SystemConfig& config);
std::pair<PositionSizing, SignalDecision> process_position_sizing(const ProcessedData& data, double equity, int current_qty, double buying_power, const SystemConfig& config);

} // namespace Core
} // namespace AlpacaTrader

#endif // STRATEGY_LOGIC_HPP


