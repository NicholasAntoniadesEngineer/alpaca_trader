#include "risk_logic.hpp"
#include "core/logging/async_logger.hpp"
#include <cmath>

namespace AlpacaTrader {
namespace Core {

namespace RiskLogic {

TradeGateResult evaluate_trade_gate(const TradeGateInput& in, const SystemConfig& config) {
    TradeGateResult out;
    out.daily_pnl = (in.initial_equity == 0.0) ? 0.0 : (in.current_equity - in.initial_equity) / in.initial_equity;
    out.pnl_ok = out.daily_pnl > config.strategy.max_daily_loss_percentage && out.daily_pnl < config.strategy.daily_profit_target_percentage;
    out.exposure_ok = in.exposure_pct <= config.strategy.max_account_exposure_percentage;
    out.allowed = out.pnl_ok && out.exposure_ok;
    return out;
}

double calculate_exposure_percentage(double current_value, double equity) {
    if (equity <= 0.0) {
        return 0.0;
    }
    return (std::abs(current_value) / equity) * 100.0;
}

} // namespace RiskLogic
} // namespace Core
} // namespace AlpacaTrader


