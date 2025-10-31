#include "signal_processor.hpp"
#include "strategy_logic.hpp"
#include "core/logging/logs/trading_logs.hpp"
#include "core/logging/logger/async_logger.hpp"
#include "core/utils/time_utils.hpp"

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

SignalProcessor::SignalProcessor(const SystemConfig& system_config) : config(system_config) {}

void SignalProcessor::process_signal_analysis(const ProcessedData& processed_data_input) 
{
    // Detect trading signals
    SignalDecision signal_decision_result = detect_trading_signals(processed_data_input, config);

    // Log candle data and enhanced signals table and detailed signal analysis
    TradingLogs::log_candle_data_table(processed_data_input.curr.open_price, processed_data_input.curr.high_price, processed_data_input.curr.low_price, processed_data_input.curr.close_price);
    TradingLogs::log_signals_table_enhanced(signal_decision_result);
    TradingLogs::log_signal_analysis_detailed(processed_data_input, signal_decision_result, config);

    // Evaluate trading filters
    FilterResult filter_result_output = evaluate_trading_filters(processed_data_input, config);

    // Log filters and summary and signal analysis results
    TradingLogs::log_filters(filter_result_output, config, processed_data_input);
    TradingLogs::log_summary(processed_data_input, signal_decision_result, filter_result_output, config.strategy.symbol);
    log_signal_analysis_results(processed_data_input, signal_decision_result, filter_result_output);
}

std::pair<PositionSizing, SignalDecision> SignalProcessor::process_position_sizing(const ProcessedData& processed_data_input, double account_equity_amount, int current_position_quantity, double buying_power_amount) {
    return AlpacaTrader::Core::process_position_sizing(PositionSizingProcessRequest(
        processed_data_input, account_equity_amount, current_position_quantity, buying_power_amount, config.strategy, config.trading_mode
    ));
}

void SignalProcessor::log_signal_analysis_results(const ProcessedData& processed_data_input, const SignalDecision& signal_decision_input, const FilterResult& filter_result_input) {
    // CSV logging for signal analysis
    try {
        log_csv_signal_data(processed_data_input, signal_decision_input, filter_result_input);
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_data_result_table("CSV logging error in signal analysis: " + std::string(exception_error.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown CSV logging error in signal analysis", false, 0);
    }
}

void SignalProcessor::log_csv_signal_data(const ProcessedData& processed_data_input, const SignalDecision& signal_decision_input, const FilterResult& filter_result_input) {
    std::string timestamp_string = TimeUtils::get_current_human_readable_time();
    if (config.trading_mode.primary_symbol.empty()) {
        throw std::runtime_error("Primary symbol is required but not configured");
    }
    std::string trading_symbol_string = config.trading_mode.primary_symbol;

    // Log signal decisions to CSV
    if (auto trade_csv_logger = AlpacaTrader::Logging::get_csv_trade_logger()) {
        trade_csv_logger->log_signal(
            timestamp_string, trading_symbol_string,
            signal_decision_input.buy, signal_decision_input.sell,
            signal_decision_input.signal_strength, signal_decision_input.signal_reason
        );
    }

    // Log filters to CSV
    if (auto trade_csv_logger = AlpacaTrader::Logging::get_csv_trade_logger()) {
        trade_csv_logger->log_filters(
            timestamp_string, trading_symbol_string, filter_result_input.atr_pass, filter_result_input.atr_ratio,
            config.strategy.use_absolute_atr_threshold ?
                config.strategy.atr_absolute_minimum_threshold :
                config.strategy.entry_signal_atr_multiplier,
            filter_result_input.vol_pass, filter_result_input.vol_ratio,
            filter_result_input.doji_pass
        );
    }

    // Log market data to CSV
    if (auto trade_csv_logger = AlpacaTrader::Logging::get_csv_trade_logger()) {
        trade_csv_logger->log_market_data(
            timestamp_string, trading_symbol_string, processed_data_input.curr.open_price, processed_data_input.curr.high_price, processed_data_input.curr.low_price, processed_data_input.curr.close_price,
            processed_data_input.curr.volume, processed_data_input.atr
        );
    }
}

// No additional helpers required

} // namespace Core
} // namespace AlpacaTrader
