#ifndef MARKET_DATA_FETCHER_HPP
#define MARKET_DATA_FETCHER_HPP

#include "configs/trader_config.hpp"
#include "data_structures.hpp"
#include "api/alpaca_client.hpp"
#include "account_manager.hpp"
#include "market_processing.hpp"
#include "core/logging/trading_logs.hpp"

namespace AlpacaTrader {
namespace Core {

class MarketDataFetcher {
public:
    MarketDataFetcher(API::AlpacaClient& client, AccountManager& account_manager, const TraderConfig& config);
    
    ProcessedData fetch_and_process_data();
    
private:
    API::AlpacaClient& client;
    AccountManager& account_manager;
    const TraderConfig& config;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MARKET_DATA_FETCHER_HPP
