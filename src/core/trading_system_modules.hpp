#ifndef TRADING_SYSTEM_MODULES_HPP
#define TRADING_SYSTEM_MODULES_HPP

#include "api/alpaca_client.hpp"
#include "core/account_manager.hpp"
#include "core/trader.hpp"
#include "logging/account_logs.hpp"
#include "threads/market_data_thread.hpp"
#include "threads/market_gate_thread.hpp"
#include "threads/account_data_thread.hpp"
#include "threads/logging_thread.hpp"
#include "threads/trader_thread.hpp"
#include <memory>

/**
 * @brief Runtime module container
 * 
 * Holds active system modules as smart pointers for centralized ownership.
 */
struct TradingSystemModules {
    // =========================================================================
    // CORE TRADING COMPONENTS
    // =========================================================================
    std::unique_ptr<AlpacaTrader::API::AlpacaClient> market_connector;    // Market data API client
    std::unique_ptr<AlpacaTrader::Core::AccountManager> portfolio_manager; // Account and portfolio management
    std::unique_ptr<AlpacaTrader::Core::Trader> trading_engine;            // Core trading logic engine
    
    // =========================================================================
    // LOGGING AND MONITORING
    // =========================================================================
    std::unique_ptr<AlpacaTrader::Logging::AccountLogs> account_dashboard;  // Account status logging
    
    // =========================================================================
    // THREADING COMPONENTS
    // =========================================================================
    std::unique_ptr<AlpacaTrader::Threads::MarketDataThread> market_data_thread;  // Market data processing thread
    std::unique_ptr<AlpacaTrader::Threads::AccountDataThread> account_data_thread; // Account data processing thread
    std::unique_ptr<AlpacaTrader::Threads::MarketGateThread> market_gate_thread;  // Market gate control thread
    std::unique_ptr<AlpacaTrader::Threads::LoggingThread> logging_thread;         // System logging thread
    std::unique_ptr<AlpacaTrader::Threads::TraderThread> trading_thread;          // Main trading execution thread
};

#endif // TRADING_SYSTEM_MODULES_HPP
