#ifndef SIGNAL_PROCESSOR_HPP
#define SIGNAL_PROCESSOR_HPP

#include "configs/system_config.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/logging/logs/trading_logs.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

class SignalProcessor {
public:
    SignalProcessor(const SystemConfig& config);

    // Signal processing methods
    void process_signal_analysis(const ProcessedData& processed_data);
    std::pair<PositionSizing, SignalDecision> process_position_sizing(const ProcessedData& processed_data, double account_equity, int current_quantity, double buying_power);

private:
    const SystemConfig& config;
    
    // Signal processing helper methods
    void log_signal_analysis_results(const ProcessedData& processed_data, const SignalDecision& signal_decision, const FilterResult& filter_result);
    void log_csv_signal_data(const ProcessedData& processed_data, const SignalDecision& signal_decision, const FilterResult& filter_result);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // SIGNAL_PROCESSOR_HPP
