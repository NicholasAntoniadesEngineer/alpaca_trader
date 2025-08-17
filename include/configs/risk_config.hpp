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
};

#endif // RISK_CONFIG_HPP


