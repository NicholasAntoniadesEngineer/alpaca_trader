#include "indicators.hpp"
#include "logging/logger/logging_macros.hpp"
#include <algorithm>
#include <cmath>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::log_message;

double compute_atr(const std::vector<double>& highs, const std::vector<double>& lows, const std::vector<double>& closes, int period, int minimum_bars_required) {
    if (static_cast<int>(highs.size()) < minimum_bars_required) {
        log_message("ATR: Insufficient bars - have " + std::to_string(highs.size()) + ", need " + std::to_string(minimum_bars_required), "");
        return 0.0;
    }
    
    int effectivePeriodValue = period;
    if (static_cast<int>(highs.size()) < period + 1) {
        effectivePeriodValue = static_cast<int>(highs.size()) - 1;
        if (effectivePeriodValue < 1) {
            effectivePeriodValue = 1;
        }
    }
    
    // CRITICAL: Validate vector sizes before accessing
    if (highs.size() != lows.size() || highs.size() != closes.size()) {
        log_message("ATR: Vector size mismatch - highs:" + std::to_string(highs.size()) +
                    ", lows:" + std::to_string(lows.size()) + ", closes:" + std::to_string(closes.size()), "");
        return 0.0;
    }

    if (highs.size() < 2) {
        log_message("ATR: Need at least 2 bars, have " + std::to_string(highs.size()), "");
        return 0.0;
    }

    // Debug: Log first few values
    if (!highs.empty()) {
        log_message("ATR: First bar - H:" + std::to_string(highs[0]) +
                   " L:" + std::to_string(lows[0]) + " C:" + std::to_string(closes[0]), "");
    }
    
    std::vector<double> true_range_values;
    try {
    for (size_t current_bar_index = 1; current_bar_index < highs.size(); ++current_bar_index) {
            // Validate index before accessing previous close
            if (current_bar_index - 1 >= closes.size()) {
                continue; // Skip this iteration if index is invalid
            }
            
        double true_range_value = std::max({highs[current_bar_index] - lows[current_bar_index], std::abs(highs[current_bar_index] - closes[current_bar_index-1]), std::abs(lows[current_bar_index] - closes[current_bar_index-1])});
        true_range_values.push_back(true_range_value);
    }
    } catch (const std::exception& true_range_calculation_exception_error) {
        return 0.0;
    } catch (...) {
        return 0.0;
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

double calculate_ema(const std::deque<MultiTimeframeBar>& bars, int ema_period) {
    try {
        if (static_cast<int>(bars.size()) < ema_period || ema_period <= 0) {
            return 0.0;
        }

        double sma = 0.0;
        for (int i = 0; i < ema_period; ++i) {
            sma += bars[bars.size() - ema_period + i].close_price;
        }
        sma /= ema_period;

        if (static_cast<int>(bars.size()) == ema_period) {
            return sma;
        }

        double ema = sma;
        double multiplier = 2.0 / (ema_period + 1.0);

        for (size_t i = bars.size() - ema_period + 1; i < bars.size(); ++i) {
            ema = (bars[i].close_price * multiplier) + (ema * (1.0 - multiplier));
        }

        return ema;
    } catch (const std::exception& e) {
        return 0.0;
    }
}

double calculate_adx(const std::deque<MultiTimeframeBar>& bars, int period) {
    try {
        if (static_cast<int>(bars.size()) < period + 1) {
            return 0.0;
        }

        double adx_sum = 0.0;
        int count = 0;

        for (size_t i = 1; i < bars.size(); ++i) {
            double true_range = std::max({bars[i].high_price - bars[i].low_price,
                                std::abs(bars[i].high_price - bars[i-1].close_price),
                                std::abs(bars[i].low_price - bars[i-1].close_price)});

            if (true_range > 0.0) {
                double dm_plus = (bars[i].high_price > bars[i-1].high_price) ?
                               std::max(bars[i].high_price - bars[i-1].high_price, 0.0) : 0.0;
                double dm_minus = (bars[i-1].low_price > bars[i].low_price) ?
                                std::max(bars[i-1].low_price - bars[i].low_price, 0.0) : 0.0;

                double di_plus = (dm_plus / true_range) * 100.0;
                double di_minus = (dm_minus / true_range) * 100.0;

                double di_sum = di_plus + di_minus;
                double dx = (di_sum > 0.0) ? (std::abs(di_plus - di_minus) / di_sum) * 100.0 : 0.0;

                if (!std::isnan(dx) && !std::isinf(dx)) {
                    adx_sum += dx;
                    count++;
                }
            }
        }

        return (count > 0) ? adx_sum / count : 0.0;
    } catch (const std::exception& e) {
        return 0.0;
    }
}

double calculate_rsi(const std::deque<MultiTimeframeBar>& bars, int period) {
    try {
        if (static_cast<int>(bars.size()) < period + 1) {
            return 50.0;
        }

        double gains = 0.0;
        double losses = 0.0;

        for (size_t i = bars.size() - period; i < bars.size(); ++i) {
            double change = bars[i].close_price - bars[i-1].close_price;
            if (change > 0) {
                gains += change;
            } else {
                losses += std::abs(change);
            }
        }

        if (losses == 0.0) {
            return 100.0;
        }
        
        double relative_strength = gains / losses;
        return 100.0 - (100.0 / (1.0 + relative_strength));
    } catch (const std::exception& e) {
        return 50.0;
    }
}

double calculate_atr(const std::deque<MultiTimeframeBar>& bars, int period) {
    try {
        if (static_cast<int>(bars.size()) < period + 1) {
            return 0.0;
        }

        std::vector<double> highs;
        std::vector<double> lows;
        std::vector<double> closes;

        highs.reserve(bars.size());
        lows.reserve(bars.size());
        closes.reserve(bars.size());

        for (const auto& bar : bars) {
            highs.push_back(bar.high_price);
            lows.push_back(bar.low_price);
            closes.push_back(bar.close_price);
        }

        return compute_atr(highs, lows, closes, period, period + 1);
    } catch (const std::exception& e) {
        return 0.0;
    }
}

double calculate_volume_ma(const std::deque<MultiTimeframeBar>& bars, int period) {
    try {
        if (static_cast<int>(bars.size()) < period) {
            return 0.0;
        }

        std::vector<double> volumes;
        volumes.reserve(bars.size());

        for (const auto& bar : bars) {
            volumes.push_back(bar.volume);
        }

        return compute_average_volume(volumes, period, 0.0);
    } catch (const std::exception& e) {
        return 0.0;
    }
}

double calculate_average_spread(const std::deque<MultiTimeframeBar>& bars, int period) {
    try {
        if (static_cast<int>(bars.size()) < period) {
            return 0.0;
        }

        double spread_sum = 0.0;
        for (size_t i = bars.size() - period; i < bars.size(); ++i) {
            spread_sum += bars[i].spread;
        }

        return spread_sum / period;
    } catch (const std::exception& e) {
        return 0.0;
    }
}

bool compute_technical_indicators(ProcessedData& processed_data, const std::vector<Bar>& bars, const SystemConfig& config) {
    // Top-level try-catch to prevent segfault
    try {
    if (bars.empty()) {
        return false;
    }
    
        // CRITICAL: Validate before accessing back()
    const Bar& current_bar = bars.back();
    processed_data.curr = current_bar;
    
    // Calculate maximum bars needed for all calculations
    // avg_atr uses: atr_calculation_bars * average_atr_comparison_multiplier + 1 (for true range calculation)
    const int atr_calculation_bars_value = config.strategy.atr_calculation_bars;
    const int average_atr_period_bars_value = atr_calculation_bars_value * config.strategy.average_atr_comparison_multiplier;
    const int max_bars_needed_for_calculations_value = average_atr_period_bars_value + 1;
    
    // Extract only the last N bars needed for calculations (optimize buffer usage)
    // This ensures we use only recent bars and avoid unnecessary processing of old data
    std::vector<Bar> bars_for_calculation_data;
    if (static_cast<int>(bars.size()) > max_bars_needed_for_calculations_value) {
        size_t start_index_value = bars.size() - static_cast<size_t>(max_bars_needed_for_calculations_value);
        bars_for_calculation_data = std::vector<Bar>(bars.begin() + start_index_value, bars.end());
    } else {
        bars_for_calculation_data = bars;
    }
    
    // Extract price data for calculations (using only bars needed)
    std::vector<double> highs, lows, closes, volumes;
    for (const auto& bar : bars_for_calculation_data) {
        highs.push_back(bar.high_price);
        lows.push_back(bar.low_price);
        closes.push_back(bar.close_price);
        volumes.push_back(bar.volume);
    }
    
    // Compute ATR - calculates continuously, can be 0.0 during initial accumulation
    processed_data.atr = compute_atr(highs, lows, closes, atr_calculation_bars_value, config.strategy.minimum_bars_for_atr_calculation);
    
    // Compute average volume
    processed_data.avg_vol = compute_average_volume(volumes, atr_calculation_bars_value, config.strategy.minimum_volume_threshold);
    
    // ATR can be 0.0 during initial accumulation - allow continuous calculation
    // Trading will be blocked by data accumulation time check until sufficient data
        return true;
    } catch (const std::exception& indicators_exception_error) {
        // Log error but return false to indicate failure
        return false;
    } catch (...) {
        // Unknown exception - return false
        return false;
    }
}

} // namespace Core
} // namespace AlpacaTrader
