#ifndef INDICATORS_HPP
#define INDICATORS_HPP

#include <vector>

double compute_atr(const std::vector<double>& highs, const std::vector<double>& lows, const std::vector<double>& closes, int period);

double compute_average_volume(const std::vector<long long>& volumes, int period);

bool detect_doji_pattern(double open, double high, double low, double close);

#endif // INDICATORS_HPP
