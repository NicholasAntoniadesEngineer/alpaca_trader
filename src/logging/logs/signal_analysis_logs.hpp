#ifndef SIGNAL_ANALYSIS_LOGS_HPP
#define SIGNAL_ANALYSIS_LOGS_HPP

#include "configs/system_config.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "trader/strategy_analysis/mth_ts_strategy.hpp"
#include <string>

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Logging {

class SignalAnalysisLogs {
public:
    static void log_signal_analysis_complete(const AlpacaTrader::Core::ProcessedData& data, const AlpacaTrader::Core::SignalDecision& signal_decision, const AlpacaTrader::Core::FilterResult& filter_result, const SystemConfig& config);
    static void log_signal_analysis_csv_data(const AlpacaTrader::Core::ProcessedData& data, const AlpacaTrader::Core::SignalDecision& signal_decision, const AlpacaTrader::Core::FilterResult& filter_result, const SystemConfig& config);
    static void log_signal_analysis_error(const std::string& error_message);
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // SIGNAL_ANALYSIS_LOGS_HPP
