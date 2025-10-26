#ifndef BARS_DATA_MANAGER_HPP
#define BARS_DATA_MANAGER_HPP

#include "configs/system_config.hpp"
#include "data_structures.hpp"
#include "api/general/api_manager.hpp"
#include "core/logging/market_data_logs.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

class BarsDataManager {
public:
    BarsDataManager(const SystemConfig& config, API::ApiManager& api_manager);

    // Bars data fetching methods
    std::vector<Bar> fetch_bars_data(const std::string& symbol) const;
    bool fetch_and_validate_bars(const std::string& symbol, std::vector<Bar>& bars_data) const;
    
    // Bars data processing methods
    bool compute_technical_indicators_from_bars(ProcessedData& processed_data, const std::vector<Bar>& bars_data) const;
    MarketSnapshot create_market_snapshot_from_bars(const std::vector<Bar>& bars_data) const;

private:
    const SystemConfig& config;
    API::ApiManager& api_manager;
    
    // Bars processing helper methods
    std::vector<double> extract_highs_from_bars(const std::vector<Bar>& bars_data) const;
    std::vector<double> extract_lows_from_bars(const std::vector<Bar>& bars_data) const;
    std::vector<double> extract_closes_from_bars(const std::vector<Bar>& bars_data) const;
    std::vector<double> extract_volumes_from_bars(const std::vector<Bar>& bars_data) const;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // BARS_DATA_MANAGER_HPP
