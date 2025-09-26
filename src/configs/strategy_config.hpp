// StrategyConfig.hpp
#ifndef STRATEGY_CONFIG_HPP
#define STRATEGY_CONFIG_HPP

struct StrategyConfig {
    int atr_period;
    double atr_multiplier_entry;
    double volume_multiplier;
    double rr_ratio;
    int avg_atr_multiplier;
    
    // Price buffer configuration for order execution
    double price_buffer_pct;                 // Percentage of entry price for buffer
    double min_price_buffer;                 // Minimum buffer amount in dollars
    double max_price_buffer;                 // Maximum buffer amount in dollars
    
    // Signal detection configuration
    bool buy_allow_equal_close;              // Allow close >= open for buy signals
    bool buy_require_higher_high;            // Require current high > previous high
    bool buy_require_higher_low;             // Require current low >= previous low
    
    bool sell_allow_equal_close;             // Allow close <= open for sell signals  
    bool sell_require_lower_low;             // Require current low < previous low
    bool sell_require_lower_high;            // Require current high <= previous high
};

#endif // STRATEGY_CONFIG_HPP


