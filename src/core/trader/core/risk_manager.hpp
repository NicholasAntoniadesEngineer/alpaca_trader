#ifndef RISK_MANAGER_HPP
#define RISK_MANAGER_HPP

#include "configs/trader_config.hpp"
#include "../data/data_structures.hpp"
#include "../analysis/risk_logic.hpp"
#include "core/logging/trading_logs.hpp"

namespace AlpacaTrader {
namespace Core {

class RiskManager {
public:
    RiskManager(const TraderConfig& config);
    
    bool validate_trading_permissions(const ProcessedData& data, double equity);
    bool check_exposure_limits(const ProcessedData& data, double equity);
    bool check_daily_limits(double current_equity, double initial_equity);
    bool check_connectivity_status();
    void log_risk_assessment(const ProcessedData& data, double equity, bool allowed);

private:
    const TraderConfig& config;
    
    RiskLogic::TradeGateInput build_risk_input(const ProcessedData& data, double equity);
    bool evaluate_risk_gate(const RiskLogic::TradeGateInput& input);
    void log_risk_conditions(const RiskLogic::TradeGateResult& result, const ProcessedData& data);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // RISK_MANAGER_HPP
