#ifndef SYSTEM_MODULES_HPP
#define SYSTEM_MODULES_HPP

#include "core/trader/data/account_manager.hpp"
#include "core/trader/trader.hpp"
#include "core/trader/market_data_coordinator.hpp"
#include "core/trader/account_data_coordinator.hpp"
#include "core/logging/logs/account_logs.hpp"
#include "api/general/api_manager.hpp"
#include "core/threads/system_threads/market_data_thread.hpp"
#include "core/threads/system_threads/market_gate_thread.hpp"
#include "core/threads/system_threads/account_data_thread.hpp"
#include "core/threads/system_threads/logging_thread.hpp"
#include "core/threads/system_threads/trader_thread.hpp"
#include <memory>

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
    std::unique_ptr<AlpacaTrader::Core::TradingOrchestrator> trading_engine;            // Core trading logic engine
    std::unique_ptr<AlpacaTrader::Core::MarketDataCoordinator> market_data_coordinator; // Market data access coordinator
    std::unique_ptr<AlpacaTrader::Core::AccountDataCoordinator> account_data_coordinator; // Account data access coordinator
    
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
