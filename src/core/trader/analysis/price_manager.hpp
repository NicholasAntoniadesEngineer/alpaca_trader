#ifndef PRICE_MANAGER_HPP
#define PRICE_MANAGER_HPP

#include "configs/system_config.hpp"
#include "api/alpaca_client.hpp"
#include "core/logging/trading_logs.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

class PriceManager {
public:
    PriceManager(API::AlpacaClient& client, const SystemConfig& config);
    
    double get_real_time_price_with_fallback(double fallback_price) const;
    
private:
    API::AlpacaClient& client;
    const SystemConfig& config;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // PRICE_MANAGER_HPP
