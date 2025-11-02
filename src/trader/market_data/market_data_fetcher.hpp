#ifndef MARKET_DATA_FETCHER_HPP
#define MARKET_DATA_FETCHER_HPP

#include "configs/system_config.hpp"
#include "trader/data_structures/data_sync_structures.hpp"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

/**
 * MarketDataFetcher - Handles data synchronization operations only.
 * This component is responsible for waiting for fresh data, checking data freshness,
 * and managing synchronization state with the MarketDataCoordinator.
 */
class MarketDataFetcher {
public:
    MarketDataFetcher(const SystemConfig& config);
    
    // Data synchronization methods
    bool wait_for_fresh_data(MarketDataSyncState& sync_state);
    bool set_sync_state_references(MarketDataSyncState& sync_state);
    bool is_data_fresh() const;
    
private:
    const SystemConfig& config;
    
    // Data synchronization state (set by TradingCoordinator)
    MarketDataSyncState* sync_state_ptr = nullptr;
    
    // Synchronization helper methods
    bool is_sync_state_valid() const;
    bool wait_for_data_availability(MarketDataSyncState& sync_state);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MARKET_DATA_FETCHER_HPP
