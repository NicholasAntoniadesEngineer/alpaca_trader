#ifndef INDICATORS_HPP
#define INDICATORS_HPP

#include <vector>
#include <deque>
#include "configs/system_config.hpp"
#include "trader/data_structures/data_structures.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

double compute_atr(const std::vector<double>& highs, const std::vector<double>& lows, const std::vector<double>& closes, int period, int minimum_bars_required);
double compute_average_volume(const std::vector<double>& volumes, int period, double minimum_threshold);
bool detect_doji_pattern(double open, double high, double low, double close);
bool compute_technical_indicators(ProcessedData& processed_data, const std::vector<Bar>& bars, const SystemConfig& config);

double calculate_ema(const std::deque<MultiTimeframeBar>& bars, int ema_period);
double calculate_adx(const std::deque<MultiTimeframeBar>& bars, int period);
double calculate_rsi(const std::deque<MultiTimeframeBar>& bars, int period);
double calculate_atr(const std::deque<MultiTimeframeBar>& bars, int period);
double calculate_volume_ma(const std::deque<MultiTimeframeBar>& bars, int period);
double calculate_average_spread(const std::deque<MultiTimeframeBar>& bars, int period);

} // namespace Core
} // namespace AlpacaTrader

#endif // INDICATORS_HPP
