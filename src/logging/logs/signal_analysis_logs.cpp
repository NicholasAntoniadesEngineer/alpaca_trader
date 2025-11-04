#include "signal_analysis_logs.hpp"
#include "trading_logs.hpp"
#include "utils/time_utils.hpp"
#include "logging/logger/csv_trade_logger.hpp"
#include <stdexcept>

namespace AlpacaTrader {
namespace Logging {

void SignalAnalysisLogs::log_signal_analysis_complete(const AlpacaTrader::Core::ProcessedData& data, const AlpacaTrader::Core::SignalDecision& signal_decision, const AlpacaTrader::Core::FilterResult& filter_result, const SystemConfig& config) {
    // Log candle data and enhanced signals table
    TradingLogs::log_candle_data_table(data.curr.open_price, data.curr.high_price, data.curr.low_price, data.curr.close_price);
    TradingLogs::log_signals_table_enhanced(signal_decision);

    // Enhanced detailed signal analysis logging
    TradingLogs::log_signal_analysis_detailed(data, signal_decision, config);

    TradingLogs::log_filters(filter_result, config, data);
    TradingLogs::log_summary(data, signal_decision, filter_result, config.strategy.symbol);
}

void SignalAnalysisLogs::log_signal_analysis_csv_data(const AlpacaTrader::Core::ProcessedData& data, const AlpacaTrader::Core::SignalDecision& signal_decision, const AlpacaTrader::Core::FilterResult& filter_result, const SystemConfig& config) {
    try {
        std::string timestamp = TimeUtils::get_current_human_readable_time();
        if (config.trading_mode.primary_symbol.empty()) {
            throw std::runtime_error("Primary symbol is required but not configured");
        }
        std::string symbol = config.trading_mode.primary_symbol;

        // Log signals to CSV
        if (auto csv = AlpacaTrader::Logging::get_logging_context()->csv_trade_logger) {
            csv->log_signal(
                timestamp, symbol, signal_decision.buy, signal_decision.sell,
                signal_decision.signal_strength, signal_decision.signal_reason
            );
        }

        // Log filters to CSV
        if (auto csv = AlpacaTrader::Logging::get_logging_context()->csv_trade_logger) {
            csv->log_filters(
                timestamp, symbol, filter_result.atr_pass, filter_result.atr_ratio,
                config.strategy.use_absolute_atr_threshold ?
                    config.strategy.atr_absolute_minimum_threshold :
                    config.strategy.entry_signal_atr_multiplier,
                filter_result.vol_pass, filter_result.vol_ratio,
                filter_result.doji_pass
            );
        }

        // Log market data to CSV
        if (auto csv = AlpacaTrader::Logging::get_logging_context()->csv_trade_logger) {
            csv->log_market_data(
                timestamp, symbol, data.curr.open_price, data.curr.high_price, data.curr.low_price, data.curr.close_price,
                data.curr.volume, data.atr
            );
        }

    } catch (const std::exception& signal_analysis_exception_error) {
        TradingLogs::log_market_data_result_table("CSV logging error in signal analysis: " + std::string(signal_analysis_exception_error.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown CSV logging error in signal analysis", false, 0);
    }
}

void SignalAnalysisLogs::log_signal_analysis_error(const std::string& error_message) {
    TradingLogs::log_market_data_result_table("Signal analysis error: " + error_message, false, 0);
}

} // namespace Logging
} // namespace AlpacaTrader
