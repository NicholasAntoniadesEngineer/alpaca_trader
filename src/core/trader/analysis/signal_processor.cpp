#include "signal_processor.hpp"
#include "strategy_logic.hpp"
#include "core/logging/logs/trading_logs.hpp"
#include "core/logging/logger/async_logger.hpp"
#include "core/utils/time_utils.hpp"

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

SignalProcessor::SignalProcessor(const SystemConfig& cfg) : config(cfg) {}

void SignalProcessor::process_signal_analysis(const ProcessedData& processed_data) 
{
    // Detect trading signals
    SignalDecision signal_decision = detect_trading_signals(processed_data, config);

    // Log candle data and enhanced signals table and detailed signal analysis
    TradingLogs::log_candle_data_table(processed_data.curr.o, processed_data.curr.h, processed_data.curr.l, processed_data.curr.c);
    TradingLogs::log_signals_table_enhanced(signal_decision);
    TradingLogs::log_signal_analysis_detailed(processed_data, signal_decision, config);

    // Evaluate trading filters
    FilterResult filter_result = evaluate_trading_filters(processed_data, config);

    // Log filters and summary and signal analysis results
    TradingLogs::log_filters(filter_result, config, processed_data);
    TradingLogs::log_summary(processed_data, signal_decision, filter_result, config.strategy.symbol);
    log_signal_analysis_results(processed_data, signal_decision, filter_result);
}

std::pair<PositionSizing, SignalDecision> SignalProcessor::process_position_sizing(const ProcessedData& processed_data, double account_equity, int current_quantity, double buying_power) {
    return AlpacaTrader::Core::process_position_sizing(PositionSizingProcessRequest(
        processed_data, account_equity, current_quantity, buying_power, config.strategy, config.trading_mode
    ));
}

void SignalProcessor::log_signal_analysis_results(const ProcessedData& processed_data, const SignalDecision& signal_decision, const FilterResult& filter_result) {
    // CSV logging for signal analysis
    try {
        log_csv_signal_data(processed_data, signal_decision, filter_result);
    } catch (const std::exception& e) {
        TradingLogs::log_market_data_result_table("CSV logging error in signal analysis: " + std::string(e.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown CSV logging error in signal analysis", false, 0);
    }
}

void SignalProcessor::log_csv_signal_data(const ProcessedData& processed_data, const SignalDecision& signal_decision, const FilterResult& filter_result) {
    std::string timestamp = TimeUtils::get_current_human_readable_time();
    if (config.trading_mode.primary_symbol.empty()) {
        throw std::runtime_error("Primary symbol is required but not configured");
    }
    std::string symbol = config.trading_mode.primary_symbol;

    // Log signals to CSV
    if (AlpacaTrader::Logging::g_csv_trade_logger) {
        AlpacaTrader::Logging::g_csv_trade_logger->log_signal(
            timestamp, symbol, signal_decision.buy, signal_decision.sell,
            signal_decision.signal_strength, signal_decision.signal_reason
        );
    }

    // Log filters to CSV
    if (AlpacaTrader::Logging::g_csv_trade_logger) {
        AlpacaTrader::Logging::g_csv_trade_logger->log_filters(
            timestamp, symbol, filter_result.atr_pass, filter_result.atr_ratio,
            config.strategy.use_absolute_atr_threshold ?
                config.strategy.atr_absolute_minimum_threshold :
                config.strategy.entry_signal_atr_multiplier,
            filter_result.vol_pass, filter_result.vol_ratio,
            filter_result.doji_pass
        );
    }

    // Log market data to CSV
    if (AlpacaTrader::Logging::g_csv_trade_logger) {
        AlpacaTrader::Logging::g_csv_trade_logger->log_market_data(
            timestamp, symbol, processed_data.curr.o, processed_data.curr.h, processed_data.curr.l, processed_data.curr.c,
            processed_data.curr.v, processed_data.atr
        );
    }
}

} // namespace Core
} // namespace AlpacaTrader
