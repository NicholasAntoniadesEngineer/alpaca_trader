#include "indicators.hpp"
#include <algorithm>
#include <cmath>

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

double compute_average_volume(const std::vector<long long>& volumes, int period) {
    if (volumes.size() < static_cast<size_t>(period)) return 0.0;
    double sum = 0.0;
    int start = static_cast<int>(volumes.size()) - period;
    for (int i = start; i < static_cast<int>(volumes.size()); ++i) sum += volumes[i];
    return sum / period;
}

bool detect_doji_pattern(double open, double high, double low, double close) {
    double body = std::abs(close - open);
    double upper = high - std::max(open, close);
    double lower = std::min(open, close) - low;
    return (upper + lower) > body;
}
