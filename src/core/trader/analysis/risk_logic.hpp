#ifndef RISK_LOGIC_HPP
#define RISK_LOGIC_HPP

#include "configs/system_config.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

namespace RiskLogic {

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

TradeGateResult evaluate_trade_gate(const TradeGateInput& in, const SystemConfig& config);

double calculate_exposure_percentage(double current_value, double equity);

} // namespace RiskLogic
} // namespace Core
} // namespace AlpacaTrader

#endif // RISK_LOGIC_HPP


