#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "configs/strategy_config.hpp"
#include "configs/risk_config.hpp"
#include "configs/timing_config.hpp"
#include "configs/flags_config.hpp"
#include "configs/ux_config.hpp"
#include "configs/logging_config.hpp"
#include "configs/target_config.hpp"
#include "configs/api_config.hpp"
#include "configs/session_config.hpp"

// Main system configuration aggregating all sub-configs
struct Config {
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

#endif // CONFIG_HPP
