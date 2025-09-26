#ifndef MAIN_HPP
#define MAIN_HPP

// =============================================================================
// MAIN.HPP - Core System Data Structures and Configuration
// =============================================================================
// Central data structures for system state, thread management, and module configuration

// =============================================================================
// SYSTEM INCLUDES
// =============================================================================

// Core system configuration and data structures
#include "configs/system_config.hpp"
#include "core/data_structures.hpp"
#include "configs/component_configs.hpp"
#include "configs/trader_config.hpp"

// API and core trading components
#include "api/alpaca_client.hpp"
#include "core/account_manager.hpp"
#include "core/trader.hpp"

// Logging system
#include "logging/account_logger.hpp"
#include "logging/async_logger.hpp"

// Threading system
#include "threads/market_data_thread.hpp"
#include "threads/account_data_thread.hpp"
#include "threads/logging_thread.hpp"
#include "threads/trader_thread.hpp"

// Namespace declarations
#include "namespaces.hpp"

// Standard library includes
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <chrono>

// =============================================================================
// SYSTEM STATE MANAGEMENT
// =============================================================================

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
    MarketSnapshot market;             // Current market data snapshot
    AccountSnapshot account;           // Current account information snapshot
    
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

// =============================================================================
// THREAD MANAGEMENT
// =============================================================================

/**
 * @brief System thread handles and performance monitoring
 * 
 * Contains thread handles and iteration counters. Implements move semantics.
 */
struct SystemThreads {
    // =========================================================================
    // THREAD HANDLES
    // =========================================================================
    std::thread market;    // Market data processing thread
    std::thread account;   // Account data processing thread
    std::thread gate;      // Market gate control thread
    std::thread trader;    // Main trading logic thread
    std::thread logger;    // Logging system thread
    
    // =========================================================================
    // PERFORMANCE MONITORING
    // =========================================================================
    std::chrono::steady_clock::time_point start_time;  // System startup timestamp
    
    /// Thread iteration counters for performance monitoring
    std::atomic<unsigned long> market_iterations{0};   // Market thread iteration count
    std::atomic<unsigned long> account_iterations{0};  // Account thread iteration count
    std::atomic<unsigned long> gate_iterations{0};     // Gate thread iteration count
    std::atomic<unsigned long> trader_iterations{0};   // Trader thread iteration count
    std::atomic<unsigned long> logger_iterations{0};   // Logger thread iteration count
    
    // =========================================================================
    // CONSTRUCTORS AND ASSIGNMENT
    // =========================================================================
    
    /**
     * @brief Default constructor
     */
    SystemThreads() : start_time(std::chrono::steady_clock::now()) {}
    
    // Copy operations explicitly deleted
    SystemThreads(const SystemThreads&) = delete;
    SystemThreads& operator=(const SystemThreads&) = delete;
    
    /**
     * @brief Move constructor
     * @param other Source object
     */
    SystemThreads(SystemThreads&& other) noexcept
        : market(std::move(other.market)),
          account(std::move(other.account)),
          gate(std::move(other.gate)),
          trader(std::move(other.trader)),
          logger(std::move(other.logger)),
          start_time(other.start_time),
          market_iterations(other.market_iterations.load()),
          account_iterations(other.account_iterations.load()),
          gate_iterations(other.gate_iterations.load()),
          trader_iterations(other.trader_iterations.load()),
          logger_iterations(other.logger_iterations.load()) {}
    
    /**
     * @brief Move assignment operator
     * @param other Source object
     * @return Reference to this object
     */
    SystemThreads& operator=(SystemThreads&& other) noexcept {
        if (this != &other) {
            market = std::move(other.market);
            account = std::move(other.account);
            gate = std::move(other.gate);
            trader = std::move(other.trader);
            logger = std::move(other.logger);
            start_time = other.start_time;
            market_iterations.store(other.market_iterations.load());
            account_iterations.store(other.account_iterations.load());
            gate_iterations.store(other.gate_iterations.load());
            trader_iterations.store(other.trader_iterations.load());
            logger_iterations.store(other.logger_iterations.load());
        }
        return *this;
    }
};

// =============================================================================
// SYSTEM CONFIGURATION AND MODULE MANAGEMENT
// =============================================================================

/**
 * @brief Configuration bundle for system components
 * 
 * Groups configuration objects for component initialization.
 */
struct TradingSystemConfigurations {
    AlpacaClientConfig market_connector;      // Market data API configuration
    AccountManagerConfig portfolio_manager;   // Account management configuration
    MarketDataThreadConfig market_data_thread; // Market data thread configuration
    AccountDataThreadConfig account_data_thread; // Account data thread configuration
};

/**
 * @brief Runtime module container
 * 
 * Holds active system modules as smart pointers for centralized ownership.
 */
struct TradingSystemModules {
    // =========================================================================
    // CORE TRADING COMPONENTS
    // =========================================================================
    std::unique_ptr<AlpacaClient> market_connector;    // Market data API client
    std::unique_ptr<AccountManager> portfolio_manager; // Account and portfolio management
    std::unique_ptr<Trader> trading_engine;            // Core trading logic engine
    
    // =========================================================================
    // LOGGING AND MONITORING
    // =========================================================================
    std::unique_ptr<AccountLogger> account_dashboard;  // Account status logging
    
    // =========================================================================
    // THREADING COMPONENTS
    // =========================================================================
    std::unique_ptr<AlpacaTrader::Threads::MarketDataThread> market_data_thread;  // Market data processing thread
    std::unique_ptr<AlpacaTrader::Threads::AccountDataThread> account_data_thread; // Account data processing thread
    std::unique_ptr<AlpacaTrader::Threads::MarketGateThread> market_gate_thread;  // Market gate control thread
    std::unique_ptr<AlpacaTrader::Threads::LoggingThread> logging_thread;         // System logging thread
    std::unique_ptr<AlpacaTrader::Threads::TraderThread> trading_thread;          // Main trading execution thread
};

#endif // MAIN_HPP
