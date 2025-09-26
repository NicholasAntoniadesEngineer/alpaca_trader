#ifndef SYSTEM_STATE_HPP
#define SYSTEM_STATE_HPP

#include "configs/system_config.hpp"
#include "core/data_structures.hpp"
#include "configs/trader_config.hpp"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory>

// Forward declarations
struct TradingSystemModules;

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
    std::atomic<bool> allow_fetch{true};    // Controls data fetching operations
    
    // =========================================================================
    // CONFIGURATION AND MODULES
    // =========================================================================
    SystemConfig config;                    // Complete system configuration
    TraderConfig trader_view;               // Trader-specific configuration view
    std::unique_ptr<TradingSystemModules> trading_modules;  // All system modules

    // =========================================================================
    // CONSTRUCTORS
    // =========================================================================
    
    /**
     * @brief Default constructor
     */
    SystemState() : trader_view(config.strategy, config.risk, config.timing,
                                config.flags, config.ux, config.logging, config.target) {}
    
    /**
     * @brief Constructor with custom configuration
     * @param initial System configuration
     */
    explicit SystemState(const SystemConfig& initial)
        : config(initial),
          trader_view(config.strategy, config.risk, config.timing,
                      config.flags, config.ux, config.logging, config.target) {}
};

#endif // SYSTEM_STATE_HPP
