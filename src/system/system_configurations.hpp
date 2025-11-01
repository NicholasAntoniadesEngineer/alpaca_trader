#ifndef TRADING_SYSTEM_CONFIGURATIONS_HPP
#define TRADING_SYSTEM_CONFIGURATIONS_HPP

#include "trader/account_management/account_manager.hpp"
#include "threads/system_threads/market_data_thread.hpp"
#include "threads/system_threads/account_data_thread.hpp"
#include "threads/system_threads/logging_thread.hpp"
#include "threads/system_threads/trader_thread.hpp"
#include "threads/thread_register.hpp"

using AccountDataThreadConfig = AlpacaTrader::Config::AccountDataThreadConfig;
using MarketDataThreadConfig = AlpacaTrader::Config::MarketDataThreadConfig;
using LoggingThreadConfig = AlpacaTrader::Config::LoggingThreadConfig;
using TraderThreadConfig = AlpacaTrader::Config::TraderThreadConfig;

/**
 * @brief Configuration bundle for system components
 * 
 * Groups configuration objects for component initialization.
 */
struct SystemConfigurations {
    AccountDataThreadConfig account_manager;   // Account management configuration (uses AccountDataThreadConfig)
    MarketDataThreadConfig market_data_thread; // Market data thread configuration
    AccountDataThreadConfig account_data_thread; // Account data thread configuration
    LoggingThreadConfig logging_thread; // Logging thread configuration
    TraderThreadConfig trader_thread; // Trader thread configuration
};

#endif // TRADING_SYSTEM_CONFIGURATIONS_HPP
