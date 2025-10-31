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
    SignalProcessor(const SystemConfig& system_config);

    // Signal processing methods
    void process_signal_analysis(const ProcessedData& processed_data_input);
    std::pair<PositionSizing, SignalDecision> process_position_sizing(const ProcessedData& processed_data_input, double account_equity_amount, int current_position_quantity, double buying_power_amount);

private:
    const SystemConfig& config;
    
    // Signal processing helper methods
    void log_signal_analysis_results(const ProcessedData& processed_data_input, const SignalDecision& signal_decision_input, const FilterResult& filter_result_input);
    void log_csv_signal_data(const ProcessedData& processed_data_input, const SignalDecision& signal_decision_input, const FilterResult& filter_result_input);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // SIGNAL_PROCESSOR_HPP
