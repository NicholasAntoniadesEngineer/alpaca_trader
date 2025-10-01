#ifndef SYSTEM_STATE_HPP
#define SYSTEM_STATE_HPP

#include "configs/system_config.hpp"
#include "core/trader/data/data_structures.hpp"
#include "configs/trader_config.hpp"
#include "core/logging/thread_logs.hpp"
#include "core/system/system_modules.hpp"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>
#include <iostream>

/**
 * @brief Central system state container
 * 
 * Contains market data, account data, configuration, and thread synchronization primitives.
 */
struct SystemState {
    // =========================================================================
    // THREAD SYNCHRONIZATION
    // =========================================================================
    std::mutex mtx;                    // Primary mutex for thread synchronization
    std::condition_variable cv;        // Condition variable for thread coordination
    
    // =========================================================================
    // MARKET AND ACCOUNT DATA
    // =========================================================================
    AlpacaTrader::Core::MarketSnapshot market;             // Current market data snapshot
    AlpacaTrader::Core::AccountSnapshot account;           // Current account information snapshot
    
    // =========================================================================
    // SYSTEM CONTROL FLAGS
    // =========================================================================
    std::atomic<bool> has_market{false};    // Indicates if market data is available
    std::atomic<bool> has_account{false};   // Indicates if account data is available
    std::atomic<bool> running{true};        // Main system running flag
    std::atomic<bool> allow_fetch{false};    // Controls data fetching operations
    
    // =========================================================================
    // DATA FRESHNESS TRACKING
    // =========================================================================
    std::atomic<std::chrono::steady_clock::time_point> market_data_timestamp{std::chrono::steady_clock::now()};  // When market data was last updated
    std::atomic<bool> market_data_fresh{false};  // Indicates if market data is fresh enough for trading
    
    // =========================================================================
    // ORDER TIMING TRACKING
    // =========================================================================
    std::atomic<std::chrono::steady_clock::time_point> last_order_timestamp{std::chrono::steady_clock::time_point::min()};  // When the last order was placed 

    // =========================================================================
    // CONFIGURATION AND MODULES
    // =========================================================================
    AlpacaTrader::Config::SystemConfig config;                    // Complete system configuration
    TraderConfig trader_view;               // Trader-specific configuration view
    std::unique_ptr<SystemModules> trading_modules;  // All system modules
    std::vector<ThreadLogs::ThreadInfo> thread_infos;  // Thread monitoring information

    // =========================================================================
    // CONSTRUCTORS
    // =========================================================================
    
    /**
     * @brief Default constructor
     */
    SystemState() : trader_view(config.strategy, config.risk, config.timing,
                                config.logging, config.target) {}
    
    /**
     * @brief Constructor with custom configuration
     * @param initial System configuration
     */
    explicit SystemState(const AlpacaTrader::Config::SystemConfig& initial)
        : config(initial),
          trader_view(config.strategy, config.risk, config.timing,
                      config.logging, config.target) {
        // Verify that the symbol was loaded correctly
        if (config.target.symbol.empty()) {
            std::cerr << "WARNING: Target symbol is empty! Config may not be loaded properly." << std::endl;
        }
    }
};

#endif // SYSTEM_STATE_HPP
