#ifndef TRADING_SYSTEM_CONFIGURATIONS_HPP
#define TRADING_SYSTEM_CONFIGURATIONS_HPP

#include "api/alpaca_client.hpp"
#include "core/account_manager.hpp"
#include "threads/system_threads/market_data_thread.hpp"
#include "threads/system_threads/account_data_thread.hpp"

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

#endif // TRADING_SYSTEM_CONFIGURATIONS_HPP
