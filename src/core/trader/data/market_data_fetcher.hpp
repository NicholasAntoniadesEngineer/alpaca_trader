#ifndef MARKET_DATA_FETCHER_HPP
#define MARKET_DATA_FETCHER_HPP

#include "configs/trader_config.hpp"
#include "data_structures.hpp"
#include "api/alpaca_client.hpp"
#include "account_manager.hpp"
#include "market_processing.hpp"
#include "core/logging/trading_logs.hpp"
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace AlpacaTrader {
namespace Core {

// Market and account data synchronization state
struct MarketDataSyncState {
    std::mutex* mtx = nullptr;
    std::condition_variable* cv = nullptr;
    MarketSnapshot* market = nullptr;
    AccountSnapshot* account = nullptr;
    std::atomic<bool>* has_market = nullptr;
    std::atomic<bool>* has_account = nullptr;
    std::atomic<bool>* running = nullptr;
    std::atomic<bool>* allow_fetch = nullptr;
};

// Data synchronization configuration for TradingOrchestrator
struct DataSyncConfig {
    std::mutex& mtx;
    std::condition_variable& cv;
    MarketSnapshot& market;
    AccountSnapshot& account;
    std::atomic<bool>& has_market;
    std::atomic<bool>& has_account;
    std::atomic<bool>& running;
    std::atomic<bool>& allow_fetch;
    
    // Constructor for easy initialization
    DataSyncConfig(std::mutex& mtx_ref, std::condition_variable& cv_ref, 
                   MarketSnapshot& market_ref, AccountSnapshot& account_ref,
                   std::atomic<bool>& has_market_ref, std::atomic<bool>& has_account_ref,
                   std::atomic<bool>& running_ref, std::atomic<bool>& allow_fetch_ref)
        : mtx(mtx_ref), cv(cv_ref), market(market_ref), account(account_ref),
          has_market(has_market_ref), has_account(has_account_ref),
          running(running_ref), allow_fetch(allow_fetch_ref) {}
};

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
    
    // Data validation methods
    bool validate_sync_state_pointers(MarketDataSyncState& sync_state) const;
    void mark_data_as_consumed(MarketDataSyncState& sync_state) const;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MARKET_DATA_FETCHER_HPP
