#ifndef MARKET_DATA_MANAGER_HPP
#define MARKET_DATA_MANAGER_HPP

#include "configs/system_config.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "trader/data_structures/data_sync_structures.hpp"
#include "trader/account_management/account_manager.hpp"
#include "api/general/api_manager.hpp"
#include "market_data_fetcher.hpp"
#include "market_data_validator.hpp"
#include "market_bars_manager.hpp"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

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
    
    // Data synchronization methods (delegated to MarketDataFetcher)
    bool wait_for_fresh_data(MarketDataSyncState& sync_state);
    bool set_sync_state_references(MarketDataSyncState& sync_state);
    bool is_data_fresh() const;
    
    // Access to sub-components
    MarketDataValidator& get_market_data_validator() { return market_data_validator; }
    MarketBarsManager& get_market_bars_manager() { return market_bars_manager; }

private:
    const SystemConfig& config;
    API::ApiManager& api_manager;
    AccountManager& account_manager;
    
    // Sub-components
    MarketDataFetcher market_data_fetcher;
    MarketDataValidator market_data_validator;
    MarketBarsManager market_bars_manager;
    
    // Market data processing helper methods
    AccountSnapshot create_account_snapshot() const;
    bool process_account_and_position_data(ProcessedData& processed_data) const;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MARKET_DATA_MANAGER_HPP
