// TraderConfig.hpp
#ifndef TRADER_CONFIG_HPP
#define TRADER_CONFIG_HPP

#include "strategy_config.hpp"
#include "risk_config.hpp"
#include "timing_config.hpp"
#include "flags_config.hpp"
#include "ux_config.hpp"
#include "logging_config.hpp"
#include "target_config.hpp"

// Grouped configuration for Trader class
struct TraderConfig {
    const StrategyConfig& strategy;
    const RiskConfig& risk;
    const TimingConfig& timing;
    const FlagsConfig& flags;
    const UXConfig& ux;
    const LoggingConfig& logging;
    const TargetConfig& target;

    TraderConfig(const StrategyConfig& strategyCfg,
                 const RiskConfig& riskCfg,
                 const TimingConfig& timingCfg,
                 const FlagsConfig& flagsCfg,
                 const UXConfig& uxCfg,
                 const LoggingConfig& loggingCfg,
                 const TargetConfig& targetCfg)
        : strategy(strategyCfg),
          risk(riskCfg),
          timing(timingCfg),
          flags(flagsCfg),
          ux(uxCfg),
          logging(loggingCfg),
          target(targetCfg) {}
};

#endif // TRADER_CONFIG_HPP
