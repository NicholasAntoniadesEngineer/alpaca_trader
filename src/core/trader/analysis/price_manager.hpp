#ifndef PRICE_MANAGER_HPP
#define PRICE_MANAGER_HPP

#include "configs/system_config.hpp"
#include "api/general/api_manager.hpp"
#include "core/logging/trading_logs.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

class PriceManager {
public:
    PriceManager(API::ApiManager& api_manager, const SystemConfig& config);
    
    double get_real_time_price_with_fallback(double fallback_price) const;
    
private:
    API::ApiManager& api_manager;
    const SystemConfig& config;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // PRICE_MANAGER_HPP
