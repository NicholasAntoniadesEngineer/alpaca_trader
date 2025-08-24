// component_configs.hpp
#ifndef COMPONENT_CONFIGS_HPP
#define COMPONENT_CONFIGS_HPP

#include "api_config.hpp"
#include "session_config.hpp"
#include "logging_config.hpp"
#include "target_config.hpp"
#include "timing_config.hpp"
#include "strategy_config.hpp"
#include "trader_config.hpp"

// Grouped config for AlpacaClient
struct AlpacaClientConfig {
    const ApiConfig& api;
    const SessionConfig& session;
    const LoggingConfig& logging;
    const TargetConfig& target;
    const TimingConfig& timing;
};

// Grouped config for AccountManager
struct AccountManagerConfig {
    const ApiConfig& api;
    const LoggingConfig& logging;
    const TargetConfig& target;
};

// Grouped config for MarketDataThread
struct MarketDataThreadConfig {
    const StrategyConfig& strategy;
    const TimingConfig& timing;
    const TargetConfig& target;
};

// Grouped config for AccountDataThread
struct AccountDataThreadConfig {
    const TimingConfig& timing;
};

// Grouped config for Trader (references underlying state config)
struct TraderConfigBundle {
    TraderConfig trader;
};

#endif // COMPONENT_CONFIGS_HPP


