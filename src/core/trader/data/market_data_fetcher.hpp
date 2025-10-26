#ifndef MARKET_DATA_FETCHER_HPP
#define MARKET_DATA_FETCHER_HPP

#include "configs/system_config.hpp"
#include "data_structures.hpp"
#include "data_sync_structures.hpp"
#include "api/general/api_manager.hpp"
#include "account_manager.hpp"
#include "market_processing.hpp"
#include "core/logging/trading_logs.hpp"
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {


class MarketDataFetcher {
public:
    MarketDataFetcher(API::ApiManager& api_manager, AccountManager& account_manager, const SystemConfig& config);
    
    // Data fetching methods
    ProcessedData fetch_and_process_data();
    std::pair<MarketSnapshot, AccountSnapshot> fetch_current_snapshots();
    
    // Data synchronization methods
    void wait_for_fresh_data(MarketDataSyncState& sync_state);
    void set_sync_state_references(MarketDataSyncState& sync_state);
    
    // Market validation methods
    bool is_market_open() const;
    bool is_data_fresh() const;
    bool validate_market_data(const MarketSnapshot& market) const;
    
private:
    // Core dependencies
    API::ApiManager& api_manager;
    AccountManager& account_manager;
    const SystemConfig& config;
    
    // Data synchronization state (set by TradingOrchestrator)
    MarketDataSyncState* sync_state_ptr = nullptr;
    
    // Cached market data for processing
    std::vector<Bar> cached_bars;
    
    // Data validation methods
    bool validate_sync_state_pointers(MarketDataSyncState& sync_state) const;
    void mark_data_as_consumed(MarketDataSyncState& sync_state) const;
    
    // Data fetching helper methods
    bool fetch_and_validate_market_bars(ProcessedData& data);
    
    // Synchronization helper methods
    bool is_sync_state_valid() const;
    bool wait_for_data_availability(MarketDataSyncState& sync_state);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MARKET_DATA_FETCHER_HPP
