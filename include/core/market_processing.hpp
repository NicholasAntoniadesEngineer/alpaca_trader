// market_processing.hpp
#ifndef MARKET_PROCESSING_HPP
#define MARKET_PROCESSING_HPP

#include "../configs/trader_config.hpp"
#include "../data/data_structures.hpp"
#include <vector>

namespace MarketProcessing {

struct IndicatorInputs {
    std::vector<double> highs;
    std::vector<double> lows;
    std::vector<double> closes;
    std::vector<long long> volumes;
};

IndicatorInputs extract_inputs_from_bars(const std::vector<Bar>& bars);
ProcessedData compute_processed_data(const std::vector<Bar>& bars, const TraderConfig& cfg);

} // namespace MarketProcessing

#endif // MARKET_PROCESSING_HPP


