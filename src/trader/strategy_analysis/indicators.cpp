#include "indicators.hpp"
#include <algorithm>
#include <cmath>

namespace AlpacaTrader {
namespace Core {

double compute_atr(const std::vector<double>& highs, const std::vector<double>& lows, const std::vector<double>& closes, int period, int minimum_bars_required) {
    if (static_cast<int>(highs.size()) < minimum_bars_required) {
        return 0.0;
    }
    
    int effectivePeriodValue = period;
    if (static_cast<int>(highs.size()) < period + 1) {
        effectivePeriodValue = static_cast<int>(highs.size()) - 1;
        if (effectivePeriodValue < 1) {
            effectivePeriodValue = 1;
        }
    }
    
    std::vector<double> true_range_values;
    for (size_t current_bar_index = 1; current_bar_index < highs.size(); ++current_bar_index) {
        double true_range_value = std::max({highs[current_bar_index] - lows[current_bar_index], std::abs(highs[current_bar_index] - closes[current_bar_index-1]), std::abs(lows[current_bar_index] - closes[current_bar_index-1])});
        true_range_values.push_back(true_range_value);
    }
    
    if (true_range_values.empty()) {
        return 0.0;
    }
    
    double true_range_sum = 0.0;
    int period_to_use_value = std::min(effectivePeriodValue, static_cast<int>(true_range_values.size()));
    int true_range_start_index = static_cast<int>(true_range_values.size()) - period_to_use_value;
    if (true_range_start_index < 0) {
        true_range_start_index = 0;
    }
    for (int true_range_index = true_range_start_index; true_range_index < static_cast<int>(true_range_values.size()); ++true_range_index) {
        true_range_sum += true_range_values[true_range_index];
    }
    
    if (period_to_use_value <= 0) {
        return 0.0;
    }
    
    return true_range_sum / period_to_use_value;
}

double compute_average_volume(const std::vector<double>& volumes, int period, double minimum_threshold) {
    if (volumes.size() < static_cast<size_t>(period)) return 0.0;
    double volume_sum = 0.0;
    int volume_start_index = static_cast<int>(volumes.size()) - period;
    
    // Calculate average over the period, including zero volumes
    for (int volume_calculation_index = volume_start_index; volume_calculation_index < static_cast<int>(volumes.size()); ++volume_calculation_index) {
        volume_sum += volumes[volume_calculation_index];
    }
    
    double average_volume_result = volume_sum / period;
    
    // If average is 0, use the minimum threshold to avoid division by zero
    // This provides a small but meaningful baseline for volume calculations
    if (average_volume_result == 0.0) {
        return minimum_threshold;
    }
    
    return average_volume_result;
}

bool detect_doji_pattern(double open, double high, double low, double close) {
    double body = std::abs(close - open);
    double upper = high - std::max(open, close);
    double lower = std::min(open, close) - low;
    return (upper + lower) > body;
}

bool compute_technical_indicators(ProcessedData& processed_data, const std::vector<Bar>& bars, const SystemConfig& config) {
    if (bars.empty()) {
        return false;
    }
    
    const Bar& current_bar = bars.back();
    processed_data.curr = current_bar;
    
    // Extract price data for calculations
    std::vector<double> highs, lows, closes, volumes;
    for (const auto& bar : bars) {
        highs.push_back(bar.high_price);
        lows.push_back(bar.low_price);
        closes.push_back(bar.close_price);
        volumes.push_back(bar.volume);
    }
    
    // Compute ATR
    processed_data.atr = compute_atr(highs, lows, closes, config.strategy.atr_calculation_period, config.strategy.minimum_bars_for_atr_calculation);
    
    // Compute average volume
    processed_data.avg_vol = compute_average_volume(volumes, config.strategy.atr_calculation_period, config.strategy.minimum_volume_threshold);
    
    if (processed_data.atr == 0.0) {
        return false;
    }
    
    return true;
}

} // namespace Core
} // namespace AlpacaTrader
