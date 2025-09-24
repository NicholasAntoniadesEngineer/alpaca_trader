// TraderConfig.hpp.
#ifndef TRADER_CONFIG_HPP
#define TRADER_CONFIG_HPP

#include "strategy_config.hpp"
#include "risk_config.hpp"
#include "timing_config.hpp"
#include "flags_config.hpp"
#include "ux_config.hpp"
#include "logging_config.hpp"
#include "target_config.hpp"

// Grouped configuration for Trader class.
struct TraderConfig {
    const StrategyConfig& strategy;
    const RiskConfig& risk;
    const TimingConfig& timing;
    const FlagsConfig& flags;
    const UXConfig& ux;
    const LoggingConfig& logging;
    const TargetConfig& target;

    TraderConfig(const StrategyConfig& strategy_cfg,
                 const RiskConfig& risk_cfg,
                 const TimingConfig& timing_cfg,
                 const FlagsConfig& flags_cfg,
                 const UXConfig& ux_cfg,
                 const LoggingConfig& logging_cfg,
                 const TargetConfig& target_cfg)
        : strategy(strategy_cfg),
          risk(risk_cfg),
          timing(timing_cfg),
          flags(flags_cfg),
          ux(ux_cfg),
          logging(logging_cfg),
          target(target_cfg) {}
};

#endif // TRADER_CONFIG_HPP
