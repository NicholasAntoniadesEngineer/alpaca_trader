#ifndef TRADING_SYSTEM_CONFIGURATIONS_HPP
#define TRADING_SYSTEM_CONFIGURATIONS_HPP

#include "core/trader/account_management/account_manager.hpp"
#include "core/threads/system_threads/market_data_thread.hpp"
#include "core/threads/system_threads/account_data_thread.hpp"
#include "core/threads/thread_register.hpp"

using AccountManagerConfig = AlpacaTrader::Config::AccountManagerConfig;
using MarketDataThreadConfig = AlpacaTrader::Config::MarketDataThreadConfig;
using AccountDataThreadConfig = AlpacaTrader::Config::AccountDataThreadConfig;

/**
 * @brief Configuration bundle for system components
 * 
 * Groups configuration objects for component initialization.
 */
struct SystemConfigurations {
    AccountManagerConfig portfolio_manager;   // Account management configuration
    MarketDataThreadConfig market_data_thread; // Market data thread configuration
    AccountDataThreadConfig account_data_thread; // Account data thread configuration
};

#endif // TRADING_SYSTEM_CONFIGURATIONS_HPP
