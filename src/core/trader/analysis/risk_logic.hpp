#ifndef RISK_LOGIC_HPP
#define RISK_LOGIC_HPP

#include "configs/trader_config.hpp"

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

TradeGateResult evaluate_trade_gate(const TradeGateInput& in, const TraderConfig& config);

} // namespace RiskLogic
} // namespace Core
} // namespace AlpacaTrader

#endif // RISK_LOGIC_HPP


