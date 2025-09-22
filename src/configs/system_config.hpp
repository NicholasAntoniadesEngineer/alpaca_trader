#ifndef SYSTEM_CONFIG_HPP
#define SYSTEM_CONFIG_HPP

#include "strategy_config.hpp"
#include "risk_config.hpp"
#include "timing_config.hpp"
#include "flags_config.hpp"
#include "ux_config.hpp"
#include "logging_config.hpp"
#include "target_config.hpp"
#include "api_config.hpp"
#include "session_config.hpp"

/**
 * Main trading system configuration.
 * Aggregates all subsystem configurations into a single structure.
 */
struct SystemConfig {
    StrategyConfig strategy;
    RiskConfig risk;
    TimingConfig timing;
    FlagsConfig flags;
    UXConfig ux;
    LoggingConfig logging;
    TargetConfig target;
    ApiConfig api;
    SessionConfig session;
};

#endif // SYSTEM_CONFIG_HPP
