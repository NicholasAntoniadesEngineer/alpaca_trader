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
    std::mutex* mtx;
    std::condition_variable* cv;
    MarketSnapshot* market;
    AccountSnapshot* account;
    std::atomic<bool>* has_market;
    std::atomic<bool>* has_account;
    std::atomic<bool>* running;
    std::atomic<bool>* allow_fetch;
    std::atomic<std::chrono::steady_clock::time_point>* market_data_timestamp;
    std::atomic<bool>* market_data_fresh;
    std::atomic<std::chrono::steady_clock::time_point>* last_order_timestamp;
    
    // No default constructor - must be initialized explicitly
    MarketDataSyncState() = delete;
    
    // Member-wise constructor for manual initialization
    MarketDataSyncState(std::mutex* mtx_ptr, std::condition_variable* cv_ptr, MarketSnapshot* market_ptr, AccountSnapshot* account_ptr,
                       std::atomic<bool>* has_market_ptr, std::atomic<bool>* has_account_ptr, std::atomic<bool>* running_ptr, std::atomic<bool>* allow_fetch_ptr,
                       std::atomic<std::chrono::steady_clock::time_point>* market_data_timestamp_ptr, std::atomic<bool>* market_data_fresh_ptr,
                       std::atomic<std::chrono::steady_clock::time_point>* last_order_timestamp_ptr)
        : mtx(mtx_ptr), cv(cv_ptr), market(market_ptr), account(account_ptr),
          has_market(has_market_ptr), has_account(has_account_ptr),
          running(running_ptr), allow_fetch(allow_fetch_ptr),
          market_data_timestamp(market_data_timestamp_ptr), market_data_fresh(market_data_fresh_ptr),
          last_order_timestamp(last_order_timestamp_ptr) {}
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
    std::mutex* mtx;
    std::condition_variable* cv;
    MarketSnapshot* market;
    AccountSnapshot* account;
    std::atomic<bool>* has_market;
    std::atomic<bool>* has_account;
    std::atomic<bool>* running;
    std::atomic<bool>* allow_fetch;
    std::atomic<std::chrono::steady_clock::time_point>* market_data_timestamp;
    std::atomic<bool>* market_data_fresh;
    std::atomic<std::chrono::steady_clock::time_point>* last_order_timestamp;
    
    // No default constructor - must be initialized explicitly
    DataSyncReferences() = delete;
    
    // Constructor for easy initialization from DataSyncConfig
    DataSyncReferences(const DataSyncConfig& config)
        : mtx(&config.mtx), cv(&config.cv), market(&config.market), account(&config.account),
          has_market(&config.has_market), has_account(&config.has_account),
          running(&config.running), allow_fetch(&config.allow_fetch),
          market_data_timestamp(&config.market_data_timestamp), market_data_fresh(&config.market_data_fresh),
          last_order_timestamp(&config.last_order_timestamp) {
        // Validate that all critical pointers are properly initialized
        if (mtx == nullptr) {
            throw std::invalid_argument("DataSyncReferences: mtx cannot be null");
        }
        if (cv == nullptr) {
            throw std::invalid_argument("DataSyncReferences: cv cannot be null");
        }
        if (market == nullptr) {
            throw std::invalid_argument("DataSyncReferences: market cannot be null");
        }
        if (account == nullptr) {
            throw std::invalid_argument("DataSyncReferences: account cannot be null");
        }
        if (has_market == nullptr) {
            throw std::invalid_argument("DataSyncReferences: has_market cannot be null");
        }
        if (has_account == nullptr) {
            throw std::invalid_argument("DataSyncReferences: has_account cannot be null");
        }
        if (running == nullptr) {
            throw std::invalid_argument("DataSyncReferences: running cannot be null");
        }
        if (allow_fetch == nullptr) {
            throw std::invalid_argument("DataSyncReferences: allow_fetch cannot be null");
        }
        if (market_data_timestamp == nullptr) {
            throw std::invalid_argument("DataSyncReferences: market_data_timestamp cannot be null");
        }
        if (market_data_fresh == nullptr) {
            throw std::invalid_argument("DataSyncReferences: market_data_fresh cannot be null");
        }
        if (last_order_timestamp == nullptr) {
            throw std::invalid_argument("DataSyncReferences: last_order_timestamp cannot be null");
        }
    }
    
    // Safe conversion to MarketDataSyncState using member-wise copy
    MarketDataSyncState to_market_data_sync_state() const {
        return MarketDataSyncState(mtx, cv, market, account, has_market, has_account, running, allow_fetch,
                                  market_data_timestamp, market_data_fresh, last_order_timestamp);
    }
};

} // namespace Core
} // namespace AlpacaTrader

#endif // DATA_SYNC_STRUCTURES_HPP
