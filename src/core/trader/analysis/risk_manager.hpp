#ifndef RISK_MANAGER_HPP
#define RISK_MANAGER_HPP

#include "configs/system_config.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/logging/logs/risk_logs.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

class RiskManager {
public:
    RiskManager(const SystemConfig& config);
    
    bool validate_trading_permissions(const ProcessedData& data, double current_equity, double initial_equity);
    bool check_exposure_limits(const ProcessedData& data, double equity);
    bool check_daily_limits(double current_equity, double initial_equity);

private:
    const SystemConfig& config;
    
    // Risk evaluation data structures
    struct TradeGateInput {
        double initial_equity;
        double current_equity;
        double exposure_pct;
    };
    
    struct TradeGateResult {
        bool allowed;
        bool pnl_ok;
        bool exposure_ok;
        double daily_pnl;
    };
    
    // Risk evaluation methods
    TradeGateInput build_risk_input(const ProcessedData& data, double current_equity, double initial_equity);
    bool evaluate_risk_gate(const TradeGateInput& input);
    TradeGateResult evaluate_trade_gate(const TradeGateInput& input);
    double calculate_exposure_percentage(double current_value, double equity);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // RISK_MANAGER_HPP
