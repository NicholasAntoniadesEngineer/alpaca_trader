// StrategyConfig.hpp
#ifndef STRATEGY_CONFIG_HPP
#define STRATEGY_CONFIG_HPP

struct StrategyConfig {
    int atr_period;
    double atr_multiplier_entry;
    double volume_multiplier;
    double rr_ratio;
    int avg_atr_multiplier;
};

#endif // STRATEGY_CONFIG_HPP


