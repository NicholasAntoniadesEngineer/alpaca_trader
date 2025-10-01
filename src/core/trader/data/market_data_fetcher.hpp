#ifndef MARKET_DATA_FETCHER_HPP
#define MARKET_DATA_FETCHER_HPP

#include "configs/trader_config.hpp"
#include "data_structures.hpp"
#include "data_sync_structures.hpp"
#include "api/alpaca_client.hpp"
#include "account_manager.hpp"
#include "market_processing.hpp"
#include "core/logging/trading_logs.hpp"
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>

namespace AlpacaTrader {
namespace Core {


class MarketDataFetcher {
public:
    MarketDataFetcher(API::AlpacaClient& client, AccountManager& account_manager, const TraderConfig& config);
    
    // Data fetching methods
    ProcessedData fetch_and_process_data();
    std::pair<MarketSnapshot, AccountSnapshot> fetch_current_snapshots();
    
    // Data synchronization methods
    void wait_for_fresh_data(MarketDataSyncState& sync_state);
    void set_sync_state_references(MarketDataSyncState& sync_state);
    
private:
    // Core dependencies
    API::AlpacaClient& client;
    AccountManager& account_manager;
    const TraderConfig& config;
    
    // Data synchronization state (set by TradingOrchestrator)
    MarketDataSyncState* sync_state_ptr = nullptr;
    
    // Cached market data for processing
    std::vector<Bar> cached_bars;
    
    // Data validation methods
    bool validate_sync_state_pointers(MarketDataSyncState& sync_state) const;
    void mark_data_as_consumed(MarketDataSyncState& sync_state) const;
    
    // Data fetching helper methods
    bool fetch_and_validate_market_bars(ProcessedData& data);
    bool compute_technical_indicators(ProcessedData& data);
    void fetch_account_and_position_data(ProcessedData& data);
    void log_position_data_and_warnings(const ProcessedData& data);
    double calculate_exposure_percentage(double current_value, double equity) const;
    
    // Synchronization helper methods
    bool is_sync_state_valid() const;
    bool wait_for_data_availability(MarketDataSyncState& sync_state);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MARKET_DATA_FETCHER_HPP
