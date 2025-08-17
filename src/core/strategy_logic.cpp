// strategy_logic.cpp
#include "core/strategy_logic.hpp"
#include "utils/indicators.hpp"
#include <cmath>

namespace StrategyLogic {

SignalDecision detect_signals(const ProcessedData& data) {
    SignalDecision decision;
    decision.buy = (data.curr.c > data.curr.o) &&
                   (data.curr.h > data.prev.h) &&
                   (data.curr.l >= data.prev.l);
    decision.sell = (data.curr.c < data.curr.o) &&
                    (data.curr.l < data.prev.l) &&
                    (data.curr.h <= data.prev.h);
    return decision;
}

FilterResult evaluate_filters(const ProcessedData& data, const TraderConfig& config) {
    FilterResult result;
    result.atr_pass = data.atr > config.strategy.atr_multiplier_entry * data.avg_atr;
    result.vol_pass = data.curr.v > config.strategy.volume_multiplier * data.avg_vol;
    result.doji_pass = !is_doji(data.curr.o, data.curr.h, data.curr.l, data.curr.c);
    result.all_pass = result.atr_pass && result.vol_pass && result.doji_pass;
    result.atr_ratio = (data.avg_atr > 0.0) ? (data.atr / data.avg_atr) : 0.0;
    result.vol_ratio = (data.avg_vol > 0.0) ? (static_cast<double>(data.curr.v) / data.avg_vol) : 0.0;
    return result;
}

PositionSizing calculate_position_sizing(const ProcessedData& data, double equity, int current_qty, const TraderConfig& config) {
    PositionSizing sizing;
    sizing.risk_amount = data.atr;
    sizing.size_multiplier = (current_qty != 0 && config.risk.allow_multiple_positions)
                                 ? config.risk.scale_in_multiplier
                                 : 1.0;
    sizing.quantity = static_cast<int>(std::floor((equity * config.risk.risk_per_trade * sizing.size_multiplier) / sizing.risk_amount));
    return sizing;
}

ExitTargets compute_exit_targets(const std::string& side, double entry_price, double risk_amount, double rr_ratio) {
    ExitTargets targets;
    if (side == "buy") {
        targets.stop_loss = entry_price - risk_amount;
        targets.take_profit = entry_price + rr_ratio * risk_amount;
    } else { // "sell"
        targets.stop_loss = entry_price + risk_amount;
        targets.take_profit = entry_price - rr_ratio * risk_amount;
    }
    return targets;
}

} // namespace StrategyLogic


