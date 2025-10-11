#include "market_processing.hpp"
#include "core/trader/analysis/indicators.hpp"

namespace AlpacaTrader {
namespace Core {

namespace MarketProcessing {

IndicatorInputs extract_inputs_from_bars(const std::vector<Bar>& bars) {
    IndicatorInputs inputs;
    inputs.highs.reserve(bars.size());
    inputs.lows.reserve(bars.size());
    inputs.closes.reserve(bars.size());
    inputs.volumes.reserve(bars.size());
    for (const Bar& b : bars) {
        inputs.highs.push_back(b.h);
        inputs.lows.push_back(b.l);
        inputs.closes.push_back(b.c);
        inputs.volumes.push_back(b.v);
    }
    return inputs;
}

ProcessedData compute_processed_data(const std::vector<Bar>& bars, const SystemConfig& cfg) {
    ProcessedData data;
    if (bars.empty()) return data;
    // Use configurable ATR calculation bars (new parameter) instead of deprecated atr_calculation_period
    const int atr_period = cfg.strategy.atr_calculation_bars;
    if (static_cast<int>(bars.size()) < atr_period + 2) return data;

    IndicatorInputs inputs = extract_inputs_from_bars(bars);
    data.atr = compute_atr(inputs.highs, inputs.lows, inputs.closes, atr_period);
    if (data.atr == 0.0) return data;
    data.avg_atr = compute_atr(inputs.highs, inputs.lows, inputs.closes, atr_period * cfg.strategy.average_atr_comparison_multiplier);
    data.avg_vol = compute_average_volume(inputs.volumes, atr_period, cfg.strategy.minimum_volume_threshold);
    
    
    data.curr = bars.back();
    data.prev = bars[bars.size() - 2];
    return data;
}

ProcessedData create_processed_data(const MarketSnapshot& market, const AccountSnapshot& account) {
    return ProcessedData(market, account);
}

} // namespace MarketProcessing
} // namespace Core
} // namespace AlpacaTrader


