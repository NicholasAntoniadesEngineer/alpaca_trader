#ifndef MARKET_PROCESSING_HPP
#define MARKET_PROCESSING_HPP

#include "configs/system_config.hpp"
#include "data_structures.hpp"
#include <vector>

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

namespace MarketProcessing {

struct IndicatorInputs {
    std::vector<double> highs;
    std::vector<double> lows;
    std::vector<double> closes;
    std::vector<double> volumes; 
};

IndicatorInputs extract_inputs_from_bars(const std::vector<Bar>& bars);
ProcessedData compute_processed_data(const std::vector<Bar>& bars, const SystemConfig& cfg);
ProcessedData create_processed_data(const MarketSnapshot& market, const AccountSnapshot& account);

} // namespace MarketProcessing
} // namespace Core
} // namespace AlpacaTrader

#endif // MARKET_PROCESSING_HPP


