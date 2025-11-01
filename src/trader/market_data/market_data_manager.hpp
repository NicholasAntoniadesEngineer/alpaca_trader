#ifndef MARKET_DATA_MANAGER_HPP
#define MARKET_DATA_MANAGER_HPP

#include "configs/system_config.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "api/general/api_manager.hpp"
#include "trader/account_management/account_manager.hpp"
#include "logging/logs/market_data_logs.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

class MarketDataManager {
public:
    MarketDataManager(const SystemConfig& config, API::ApiManager& api_manager, AccountManager& account_manager);

    // Market data fetching methods
    ProcessedData fetch_and_process_market_data();
    std::pair<MarketSnapshot, AccountSnapshot> fetch_current_snapshots();
    QuoteData fetch_real_time_quote_data(const std::string& symbol) const;
    
    // Market data processing methods
    void process_account_and_position_data(ProcessedData& processed_data) const;

private:
    const SystemConfig& config;
    API::ApiManager& api_manager;
    AccountManager& account_manager;
    
    // Market data processing helper methods
    MarketSnapshot create_market_snapshot_from_bars(const std::vector<Bar>& bars_data) const;
    AccountSnapshot create_account_snapshot() const;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MARKET_DATA_MANAGER_HPP
