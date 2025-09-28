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
#include "orders_config.hpp"
#include "core/logging/async_logger.hpp"
#include "core/system/system_state.hpp"
#include "core/system/system_modules.hpp"
#include "core/system/system_configurations.hpp"
#include "thread_register_config.hpp"

// Grouped config for Trader (references underlying state config)
struct TraderConfigBundle {
    TraderConfig trader;
};

// Trading system module creation and configuration functions
SystemConfigurations create_trading_configurations(const SystemState& state);
SystemModules create_trading_modules(SystemState& state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger);
void configure_trading_modules(SystemState& system_state, SystemModules& modules);

#endif // COMPONENT_CONFIGS_HPP


