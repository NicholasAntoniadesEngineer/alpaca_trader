// strategy_logic.hpp
#ifndef STRATEGY_LOGIC_HPP
#define STRATEGY_LOGIC_HPP

#include "../configs/trader_config.hpp"
#include "../data/data_structures.hpp"

namespace StrategyLogic {

struct SignalDecision {
    bool buy{false};
    bool sell{false};
};

struct FilterResult {
    bool atr_pass{false};
    bool vol_pass{false};
    bool doji_pass{false};
    bool all_pass{false};
    double atr_ratio{0.0};
    double vol_ratio{0.0};
};

struct PositionSizing {
    int quantity{0};
    double risk_amount{0.0};
    double size_multiplier{1.0};
    // Debug information for logging
    int risk_based_qty{0};
    int exposure_based_qty{0};
    int max_value_qty{0};
    int buying_power_qty{0};
};

struct ExitTargets {
    double stop_loss{0.0};
    double take_profit{0.0};
};

SignalDecision detect_signals(const ProcessedData& data, const TraderConfig& config);
FilterResult evaluate_filters(const ProcessedData& data, const TraderConfig& config);
PositionSizing calculate_position_sizing(const ProcessedData& data, double equity, int current_qty, const TraderConfig& config, double buying_power = 0.0);
ExitTargets compute_exit_targets(const std::string& side, double entry_price, double risk_amount, double rr_ratio);

} // namespace StrategyLogic

#endif // STRATEGY_LOGIC_HPP


