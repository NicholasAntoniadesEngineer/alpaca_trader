// RiskConfig.hpp
#ifndef RISK_CONFIG_HPP
#define RISK_CONFIG_HPP

struct RiskConfig {
    double risk_per_trade;
    double daily_max_loss;
    double daily_profit_target;
    double max_exposure_pct;
    bool allow_multiple_positions;
    int max_layers;
    double scale_in_multiplier;
    bool close_on_reverse;
    // Position sizing safety factors
    double buying_power_usage_factor{0.8};  // Use 80% of available buying power
    double buying_power_validation_factor{1.25}; // Require 125% of position value
};

#endif // RISK_CONFIG_HPP


