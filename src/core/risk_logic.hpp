#ifndef RISK_LOGIC_HPP
#define RISK_LOGIC_HPP

#include "configs/trader_config.hpp"

namespace AlpacaTrader {
namespace Core {

namespace RiskLogic {

struct TradeGateInput {
    double initial_equity{0.0};
    double current_equity{0.0};
    double exposure_pct{0.0};
    bool core_trading_hours{false};
};

struct TradeGateResult {
    bool allowed{false};
    bool hours_ok{false};
    bool pnl_ok{false};
    bool exposure_ok{false};
    double daily_pnl{0.0};
};

TradeGateResult evaluate_trade_gate(const TradeGateInput& in, const TraderConfig& config);

} // namespace RiskLogic
} // namespace Core
} // namespace AlpacaTrader

#endif // RISK_LOGIC_HPP


