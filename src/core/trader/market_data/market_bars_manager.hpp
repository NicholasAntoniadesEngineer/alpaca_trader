#ifndef MARKET_BARS_MANAGER_HPP
#define MARKET_BARS_MANAGER_HPP

#include "configs/system_config.hpp"
#include "core/trader/data_structures/data_structures.hpp"
#include "api/general/api_manager.hpp"
#include "core/logging/logs/market_data_logs.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

class MarketBarsManager {
public:
    MarketBarsManager(const SystemConfig& config, API::ApiManager& api_manager);

    // Bars data fetching methods
    std::vector<Bar> fetch_bars_data(const std::string& symbol) const;
    bool fetch_and_validate_bars(const std::string& symbol, std::vector<Bar>& bars_data) const;
    std::vector<Bar> fetch_historical_market_data(const MarketDataFetchRequest& fetch_request) const;
    bool has_sufficient_bars_for_calculations(const std::vector<Bar>& historical_bars, int required_bars) const;
    
    // Bars data processing methods
    bool compute_technical_indicators_from_bars(ProcessedData& processed_data, const std::vector<Bar>& bars_data) const;
    ProcessedData compute_processed_data_from_bars(const std::vector<Bar>& bars_data) const;
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

#endif // MARKET_BARS_MANAGER_HPP
