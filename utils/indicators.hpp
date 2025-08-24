#ifndef INDICATORS_HPP
#define INDICATORS_HPP

#include <vector>

double calculate_atr(const std::vector<double>& highs, const std::vector<double>& lows, const std::vector<double>& closes, int period);

double calculate_avg_volume(const std::vector<long long>& volumes, int period);

bool is_doji(double open, double high, double low, double close);

#endif // INDICATORS_HPP
