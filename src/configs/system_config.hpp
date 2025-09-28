#ifndef SYSTEM_CONFIG_HPP
#define SYSTEM_CONFIG_HPP

#include "strategy_config.hpp"
#include "risk_config.hpp"
#include "timing_config.hpp"
#include "logging_config.hpp"
#include "target_config.hpp"
#include "api_config.hpp"
#include "session_config.hpp"
#include "orders_config.hpp"
#include "thread_register_config.hpp"

namespace AlpacaTrader {
namespace Config {

/**
 * Main trading system configuration.
 * Aggregates all subsystem configurations into a single structure.
 */
struct SystemConfig {
    StrategyConfig strategy;
    RiskConfig risk;
    TimingConfig timing;
    LoggingConfig logging;
    TargetConfig target;
    ApiConfig api;
    SessionConfig session;
    OrdersConfig orders;
    ThreadConfigRegistry thread_registry;
};

} // namespace Config
} // namespace AlpacaTrader

#endif // SYSTEM_CONFIG_HPP
