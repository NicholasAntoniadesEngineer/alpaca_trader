#include "market_processing.hpp"
#include "core/trader/analysis/indicators.hpp"
#include "core/logging/logs/trading_logs.hpp"
#include "core/logging/logs/market_data_logs.hpp"
#include <cmath>
#include <stdexcept>

namespace AlpacaTrader {
namespace Core {

namespace MarketProcessing {

IndicatorInputs extract_inputs_from_bars(const std::vector<Bar>& bars) {
    IndicatorInputs inputs;
    inputs.highs.reserve(bars.size());
    inputs.lows.reserve(bars.size());
    inputs.closes.reserve(bars.size());
    inputs.volumes.reserve(bars.size());
    for (const Bar& current_bar : bars) {
        inputs.highs.push_back(current_bar.high_price);
        inputs.lows.push_back(current_bar.low_price);
        inputs.closes.push_back(current_bar.close_price);
        inputs.volumes.push_back(current_bar.volume);
    }
    return inputs;
}

ProcessedData compute_processed_data(const std::vector<Bar>& bars, const SystemConfig& cfg) {
    ProcessedData data;
    if (bars.empty()) return data;
    // Use configurable ATR calculation bars for period calculations
    const int atr_period = cfg.strategy.atr_calculation_bars;
    if (static_cast<int>(bars.size()) < atr_period + 2) return data;

    IndicatorInputs inputs = extract_inputs_from_bars(bars);
    data.atr = AlpacaTrader::Core::compute_atr(inputs.highs, inputs.lows, inputs.closes, atr_period);
    if (data.atr == 0.0) return data;
    data.avg_atr = AlpacaTrader::Core::compute_atr(inputs.highs, inputs.lows, inputs.closes, atr_period * cfg.strategy.average_atr_comparison_multiplier);
    data.avg_vol = AlpacaTrader::Core::compute_average_volume(inputs.volumes, atr_period, cfg.strategy.minimum_volume_threshold);
    
    // Defensive checks before accessing tail elements
    if (bars.size() < 2) {
        throw std::runtime_error("Insufficient bars for processed data tail access");
    }
    
    data.curr = bars.back();
    data.prev = bars[bars.size() - 2];
    return data;
}

ProcessedData create_processed_data(const MarketSnapshot& market, const AccountSnapshot& account) {
    return ProcessedData(market, account);
}

} // namespace MarketProcessing

ProcessedData::ProcessedData()
    : atr(0.0), avg_atr(0.0), avg_vol(0.0), curr(), prev(), pos_details(), open_orders(0), exposure_pct(0.0), is_doji(false) {}

} // namespace Core
} // namespace AlpacaTrader


