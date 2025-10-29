#include "indicators.hpp"
#include "core/logging/logs/market_data_logs.hpp"
#include <algorithm>
#include <cmath>

using AlpacaTrader::Logging::MarketDataLogs;

namespace AlpacaTrader {
namespace Core {

double compute_atr(const std::vector<double>& highs, const std::vector<double>& lows, const std::vector<double>& closes, int period) {
    if (highs.size() < static_cast<size_t>(period + 1)) return 0.0;
    std::vector<double> trs;
    for (size_t i = 1; i < highs.size(); ++i) {
        double tr = std::max({highs[i] - lows[i], std::abs(highs[i] - closes[i-1]), std::abs(lows[i] - closes[i-1])});
        trs.push_back(tr);
    }
    double sum = 0.0;
    int start = static_cast<int>(trs.size()) - period;
    for (int i = start; i < static_cast<int>(trs.size()); ++i) sum += trs[i];
    return sum / period;
}

double compute_average_volume(const std::vector<double>& volumes, int period, double minimum_threshold) {
    if (volumes.size() < static_cast<size_t>(period)) return 0.0;
    double sum = 0.0;
    int start = static_cast<int>(volumes.size()) - period;
    
    // Calculate average over the period, including zero volumes
    for (int i = start; i < static_cast<int>(volumes.size()); ++i) {
        sum += volumes[i];
    }
    
    double avg = sum / period;
    
    // If average is 0, use the minimum threshold to avoid division by zero
    // This provides a small but meaningful baseline for volume calculations
    if (avg == 0.0) {
        return minimum_threshold;
    }
    
    return avg;
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
        highs.push_back(bar.h);
        lows.push_back(bar.l);
        closes.push_back(bar.c);
        volumes.push_back(bar.v);
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
