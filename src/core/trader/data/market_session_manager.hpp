#ifndef MARKET_SESSION_MANAGER_HPP
#define MARKET_SESSION_MANAGER_HPP

#include "configs/system_config.hpp"
#include "api/general/api_manager.hpp"
#include "core/logging/trading_logs.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

class MarketSessionManager {
public:
    MarketSessionManager(const SystemConfig& config, API::ApiManager& api_manager);

    // Market session methods
    bool is_market_open() const;

private:
    const SystemConfig& config;
    API::ApiManager& api_manager;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MARKET_SESSION_MANAGER_HPP
