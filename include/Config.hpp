#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include "configs/api_config.hpp"
#include "configs/target_config.hpp"
#include "configs/strategy_config.hpp"
#include "configs/risk_config.hpp"
#include "configs/timing_config.hpp"
#include "configs/session_config.hpp"
#include "configs/flags_config.hpp"
#include "configs/ux_config.hpp"
#include "configs/logging_config.hpp"

// Aggregate config
struct Config {
    ApiConfig api;
    TargetConfig target;
    StrategyConfig strategy;
    RiskConfig risk;
    TimingConfig timing;
    SessionConfig session;
    FlagsConfig flags;
    UXConfig ux;
    LoggingConfig logging;

    Config() = default; // No defaults; values must be provided at runtime (CSV/env)
};

// (Removed module-specific view structs per user request)

#endif // CONFIG_HPP
