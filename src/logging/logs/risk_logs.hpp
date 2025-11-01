#ifndef RISK_LOGS_HPP
#define RISK_LOGS_HPP

#include "configs/system_config.hpp"
#include "trader/data_structures/data_structures.hpp"
#include <string>

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Logging {

class RiskLogs {
public:
    // Risk assessment logging
    static void log_risk_assessment(const AlpacaTrader::Core::ProcessedData& data, bool allowed, const SystemConfig& config, double current_equity, double initial_equity);
    static void log_risk_conditions(double daily_pnl, double exposure_pct, bool allowed, const SystemConfig& config);
    static void log_risk_status(bool allowed, const std::string& reason);
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // RISK_LOGS_HPP
