#ifndef SYSTEM_STATE_HPP
#define SYSTEM_STATE_HPP

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>
#include "system/system_modules.hpp"
#include "system/system_monitor.hpp"
#include "configs/system_config.hpp"
#include "threads/thread_logic/thread_manager.hpp"
#include "logging/logs/thread_logs.hpp"
#include "logging/logger/async_logger.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "utils/connectivity_manager.hpp"

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
    std::atomic<bool> shutdown_requested{false};  // Can be set to trigger graceful shutdown
    
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
    const AlpacaTrader::Config::SystemConfig& trader_view = config;  // Trader-specific configuration view
    std::unique_ptr<SystemModules> trading_modules;  // All system modules
    std::vector<ThreadLogs::ThreadInfo> thread_infos;  // Thread monitoring information
    AlpacaTrader::Core::ThreadManagerState thread_manager_state;  // Thread management state
    AlpacaTrader::Monitoring::SystemMonitor system_monitor;  // System monitoring state
    ConnectivityManager connectivity_manager;  // System connectivity state
    std::shared_ptr<AlpacaTrader::Logging::LoggingContext> logging_context;  // Logging context

    // =========================================================================
    // CONSTRUCTORS
    // =========================================================================

    /**
     * @brief Constructor with custom configuration
     * @param initial System configuration
     */
    explicit SystemState(const AlpacaTrader::Config::SystemConfig& initial)
        : config(initial), connectivity_manager(config.timing) {
        if (config.strategy.symbol.empty()) {
            throw std::runtime_error("Target symbol is required but not configured");
        }
    }
};

#endif // SYSTEM_STATE_HPP
