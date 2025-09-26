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
    double buying_power_usage_factor;  // Use percentage of available buying power
    double buying_power_validation_factor; // Require percentage of position value
    // Maximum value per trade constraint
    double max_value_per_trade; // Maximum dollar value per individual trade (0 = no limit)
    
    // Precision configuration for logging and display
    int percentage_precision;           // Decimal places for percentages (e.g., exposure_pct)
    int exposure_precision;             // Decimal places for exposure values
    int factor_precision;               // Decimal places for risk factors
    int currency_precision;             // Decimal places for currency amounts
};

#endif // RISK_CONFIG_HPP


