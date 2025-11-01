#ifndef INDICATORS_HPP
#define INDICATORS_HPP

#include <vector>
#include "configs/system_config.hpp"
#include "trader/data_structures/data_structures.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

double compute_atr(const std::vector<double>& highs, const std::vector<double>& lows, const std::vector<double>& closes, int period);
double compute_average_volume(const std::vector<double>& volumes, int period, double minimum_threshold);
bool detect_doji_pattern(double open, double high, double low, double close);
bool compute_technical_indicators(ProcessedData& processed_data, const std::vector<Bar>& bars, const SystemConfig& config);

} // namespace Core
} // namespace AlpacaTrader

#endif // INDICATORS_HPP
