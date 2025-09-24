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
    const TimingConfig& timing;
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

// Forward declarations
struct SystemState;
struct TradingSystemModules;
struct TradingSystemConfigurations;

// Trading system module creation and configuration functions
TradingSystemConfigurations create_trading_configurations(const SystemState& state);
TradingSystemModules create_trading_modules(SystemState& state);
void configure_trading_modules(SystemState& system_state, TradingSystemModules& modules);

#endif // COMPONENT_CONFIGS_HPP


