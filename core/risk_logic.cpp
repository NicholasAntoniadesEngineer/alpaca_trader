// risk_logic.cpp
#include "core/risk_logic.hpp"
#include "../logging/async_logger.hpp"

namespace RiskLogic {

TradeGateResult evaluate_trade_gate(const TradeGateInput& in, const TraderConfig& config) {
    TradeGateResult out;
    out.hours_ok = in.core_trading_hours;
    out.daily_pnl = (in.initial_equity == 0.0) ? 0.0 : (in.current_equity - in.initial_equity) / in.initial_equity;
    out.pnl_ok = out.daily_pnl > config.risk.daily_max_loss && out.daily_pnl < config.risk.daily_profit_target;
    out.exposure_ok = in.exposure_pct <= config.risk.max_exposure_pct;
    out.allowed = out.hours_ok && out.pnl_ok && out.exposure_ok;
    return out;
}

} // namespace RiskLogic


