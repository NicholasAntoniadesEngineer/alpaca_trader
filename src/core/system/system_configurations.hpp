#ifndef TRADING_SYSTEM_CONFIGURATIONS_HPP
#define TRADING_SYSTEM_CONFIGURATIONS_HPP

#include "api/alpaca_client.hpp"
#include "core/trader/account_manager.hpp"
#include "core/threads/system_threads/market_data_thread.hpp"
#include "core/threads/system_threads/account_data_thread.hpp"
#include "configs/thread_register_config.hpp"

using AlpacaClientConfig = AlpacaTrader::Config::AlpacaClientConfig;
using AccountManagerConfig = AlpacaTrader::Config::AccountManagerConfig;
using MarketDataThreadConfig = AlpacaTrader::Config::MarketDataThreadConfig;
using AccountDataThreadConfig = AlpacaTrader::Config::AccountDataThreadConfig;

/**
 * @brief Configuration bundle for system components
 * 
 * Groups configuration objects for component initialization.
 */
struct SystemConfigurations {
    AlpacaClientConfig market_connector;      // Market data API configuration
    AccountManagerConfig portfolio_manager;   // Account management configuration
    MarketDataThreadConfig market_data_thread; // Market data thread configuration
    AccountDataThreadConfig account_data_thread; // Account data thread configuration
};

#endif // TRADING_SYSTEM_CONFIGURATIONS_HPP
