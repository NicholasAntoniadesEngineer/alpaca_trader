#ifndef RISK_LOGS_HPP
#define RISK_LOGS_HPP

#include "configs/system_config.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/trader/analysis/risk_logic.hpp"
#include <string>

namespace AlpacaTrader {
namespace Logging {

class RiskLogs {
public:
    // Risk assessment logging
    static void log_risk_assessment(const AlpacaTrader::Core::ProcessedData& data, double equity, bool allowed, const SystemConfig& config);
    static void log_risk_conditions(const AlpacaTrader::Core::RiskLogic::TradeGateResult& result, const AlpacaTrader::Core::ProcessedData& data, const SystemConfig& config);
    static void log_risk_status(bool allowed, const std::string& reason = "");
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // RISK_LOGS_HPP
