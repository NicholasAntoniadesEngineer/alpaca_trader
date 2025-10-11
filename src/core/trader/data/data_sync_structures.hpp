#ifndef DATA_SYNC_STRUCTURES_HPP
#define DATA_SYNC_STRUCTURES_HPP

#include "data_structures.hpp"
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

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
    std::atomic<std::chrono::steady_clock::time_point>* market_data_timestamp = nullptr;
    std::atomic<bool>* market_data_fresh = nullptr;
    std::atomic<std::chrono::steady_clock::time_point>* last_order_timestamp = nullptr;
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
    std::atomic<std::chrono::steady_clock::time_point>& market_data_timestamp;
    std::atomic<bool>& market_data_fresh;
    std::atomic<std::chrono::steady_clock::time_point>& last_order_timestamp;
    
    // Constructor for easy initialization
    DataSyncConfig(std::mutex& mtx_ref, std::condition_variable& cv_ref, 
                   MarketSnapshot& market_ref, AccountSnapshot& account_ref,
                   std::atomic<bool>& has_market_ref, std::atomic<bool>& has_account_ref,
                   std::atomic<bool>& running_ref, std::atomic<bool>& allow_fetch_ref,
                   std::atomic<std::chrono::steady_clock::time_point>& timestamp_ref,
                   std::atomic<bool>& fresh_ref, std::atomic<std::chrono::steady_clock::time_point>& last_order_ref)
        : mtx(mtx_ref), cv(cv_ref), market(market_ref), account(account_ref),
          has_market(has_market_ref), has_account(has_account_ref),
          running(running_ref), allow_fetch(allow_fetch_ref),
          market_data_timestamp(timestamp_ref), market_data_fresh(fresh_ref), last_order_timestamp(last_order_ref) {}
};

// Data synchronization references for TradingEngine
struct DataSyncReferences {
    std::mutex* mtx = nullptr;
    std::condition_variable* cv = nullptr;
    MarketSnapshot* market = nullptr;
    AccountSnapshot* account = nullptr;
    std::atomic<bool>* has_market = nullptr;
    std::atomic<bool>* has_account = nullptr;
    std::atomic<bool>* running = nullptr;
    std::atomic<bool>* allow_fetch = nullptr;
    std::atomic<std::chrono::steady_clock::time_point>* market_data_timestamp = nullptr;
    std::atomic<bool>* market_data_fresh = nullptr;
    std::atomic<std::chrono::steady_clock::time_point>* last_order_timestamp = nullptr;
    
    // Default constructor
    DataSyncReferences() = default;
    
    // Constructor for easy initialization from DataSyncConfig
    DataSyncReferences(const DataSyncConfig& config)
        : mtx(&config.mtx), cv(&config.cv), market(&config.market), account(&config.account),
          has_market(&config.has_market), has_account(&config.has_account),
          running(&config.running), allow_fetch(&config.allow_fetch),
          market_data_timestamp(&config.market_data_timestamp), market_data_fresh(&config.market_data_fresh),
          last_order_timestamp(&config.last_order_timestamp) {
        // Validate that critical pointers are properly initialized
        if (market_data_timestamp == nullptr) {
            throw std::invalid_argument("DataSyncReferences: market_data_timestamp cannot be null");
        }
    }
};

} // namespace Core
} // namespace AlpacaTrader

#endif // DATA_SYNC_STRUCTURES_HPP
