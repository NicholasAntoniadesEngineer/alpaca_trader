// StrategyConfig.hpp
#ifndef STRATEGY_CONFIG_HPP
#define STRATEGY_CONFIG_HPP

struct StrategyConfig {
    int atr_period;
    double atr_multiplier_entry;
    double volume_multiplier;
    double rr_ratio;
    int avg_atr_multiplier;
    
    // Signal detection configuration
    bool buy_allow_equal_close{true};        // Allow close >= open for buy signals
    bool buy_require_higher_high{true};      // Require current high > previous high
    bool buy_require_higher_low{false};      // Require current low >= previous low
    
    bool sell_allow_equal_close{true};       // Allow close <= open for sell signals  
    bool sell_require_lower_low{true};       // Require current low < previous low
    bool sell_require_lower_high{false};     // Require current high <= previous high
};

#endif // STRATEGY_CONFIG_HPP


