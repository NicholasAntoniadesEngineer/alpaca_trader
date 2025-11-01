#include "indicators.hpp"
#include "logging/logs/market_data_logs.hpp"
#include <algorithm>
#include <cmath>

using AlpacaTrader::Logging::MarketDataLogs;

namespace AlpacaTrader {
namespace Core {

double compute_atr(const std::vector<double>& highs, const std::vector<double>& lows, const std::vector<double>& closes, int period) {
    if (highs.size() < static_cast<size_t>(period + 1)) return 0.0;
    std::vector<double> true_range_values;
    for (size_t current_bar_index = 1; current_bar_index < highs.size(); ++current_bar_index) {
        double true_range_value = std::max({highs[current_bar_index] - lows[current_bar_index], std::abs(highs[current_bar_index] - closes[current_bar_index-1]), std::abs(lows[current_bar_index] - closes[current_bar_index-1])});
        true_range_values.push_back(true_range_value);
    }
    double true_range_sum = 0.0;
    int true_range_start_index = static_cast<int>(true_range_values.size()) - period;
    for (int true_range_index = true_range_start_index; true_range_index < static_cast<int>(true_range_values.size()); ++true_range_index) true_range_sum += true_range_values[true_range_index];
    return true_range_sum / period;
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
    MarketDataLogs::log_market_data_attempt_table("Computing indicators", config.logging.log_file);
    
    if (bars.empty()) {
        MarketDataLogs::log_market_data_result_table("Indicator computation failed - no bars", false, 0, config.logging.log_file);
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
    processed_data.atr = compute_atr(highs, lows, closes, config.strategy.atr_calculation_period);
    
    // Compute average volume
    processed_data.avg_vol = compute_average_volume(volumes, config.strategy.atr_calculation_period, config.strategy.minimum_volume_threshold);
    
    if (processed_data.atr == 0.0) {
        MarketDataLogs::log_market_data_result_table("Indicator computation failed - ATR is zero", false, 0, config.logging.log_file);
        return false;
    }
    
    MarketDataLogs::log_market_data_result_table("Indicator computation successful", true, processed_data.atr, config.logging.log_file);
    return true;
}

} // namespace Core
} // namespace AlpacaTrader
