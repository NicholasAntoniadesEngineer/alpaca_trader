#ifndef SYSTEM_MODULES_HPP
#define SYSTEM_MODULES_HPP

#include <memory>
#include "api/general/api_manager.hpp"
#include "threads/system_threads/account_data_thread.hpp"
#include "threads/system_threads/logging_thread.hpp"
#include "threads/system_threads/market_data_thread.hpp"
#include "threads/system_threads/market_gate_thread.hpp"
#include "threads/system_threads/trader_thread.hpp"
#include "logging/logs/account_logs.hpp"
#include "trader/account_management/account_manager.hpp"
#include "trader/coordinators/account_data_coordinator.hpp"
#include "trader/coordinators/market_data_coordinator.hpp"
#include "trader/coordinators/market_gate_coordinator.hpp"
#include "trader/coordinators/trading_coordinator.hpp"
#include "trader/trading_logic/trading_logic.hpp"

/**
 * @brief Runtime module container
 * 
 * Holds active system modules as smart pointers for centralized ownership.
 */
struct SystemModules {
    // =========================================================================
    // CORE TRADING COMPONENTS
    // =========================================================================
    std::unique_ptr<AlpacaTrader::API::ApiManager> api_manager;           // Multi-provider API manager
    std::unique_ptr<AlpacaTrader::Core::AccountManager> portfolio_manager; // Account and portfolio management
    std::unique_ptr<AlpacaTrader::Core::TradingLogic> trading_logic;    // Core trading logic engine
    std::unique_ptr<AlpacaTrader::Core::TradingCoordinator> trading_coordinator; // Trading thread-safe interface
    std::unique_ptr<AlpacaTrader::Core::MarketDataCoordinator> market_data_coordinator; // Market data access coordinator
    std::unique_ptr<AlpacaTrader::Core::AccountDataCoordinator> account_data_coordinator; // Account data access coordinator
    std::unique_ptr<AlpacaTrader::Core::MarketGateCoordinator> market_gate_coordinator; // Market gate control coordinator
    
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

#endif // SYSTEM_MODULES_HPP
