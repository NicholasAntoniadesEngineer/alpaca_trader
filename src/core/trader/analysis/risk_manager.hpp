#ifndef RISK_MANAGER_HPP
#define RISK_MANAGER_HPP

#include "configs/system_config.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/trader/analysis/risk_logic.hpp"
#include "core/logging/risk_logs.hpp"

namespace AlpacaTrader {
namespace Core {

class RiskManager {
public:
    RiskManager(const SystemConfig& config);
    
    bool validate_trading_permissions(const ProcessedData& data, double equity);
    bool check_exposure_limits(const ProcessedData& data, double equity);
    bool check_daily_limits(double current_equity, double initial_equity);
    void log_risk_assessment(const ProcessedData& data, double equity, bool allowed);

private:
    const SystemConfig& config;
    
    RiskLogic::TradeGateInput build_risk_input(const ProcessedData& data, double equity);
    bool evaluate_risk_gate(const RiskLogic::TradeGateInput& input);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // RISK_MANAGER_HPP
