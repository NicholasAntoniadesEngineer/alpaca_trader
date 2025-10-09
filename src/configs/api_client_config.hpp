#ifndef API_CLIENT_CONFIG_HPP
#define API_CLIENT_CONFIG_HPP

#include "api_config.hpp"
#include "logging_config.hpp"
#include "timing_config.hpp"
#include "strategy_config.hpp"

namespace AlpacaTrader {
namespace Config {

/**
 * @brief Configuration structure for API client components
 * 
 * Contains references to all configuration objects needed by API clients.
 */
struct AlpacaClientConfig {
    const ApiConfig& api;
    const LoggingConfig& logging;
    const TimingConfig& timing;
    const StrategyConfig& strategy;
};

} // namespace Config
} // namespace AlpacaTrader

#endif // API_CLIENT_CONFIG_HPP
