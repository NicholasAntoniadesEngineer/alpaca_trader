#include "signal_processor.hpp"
#include "strategy_logic.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/logging/async_logger.hpp"
#include "core/utils/time_utils.hpp"

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

SignalProcessor::SignalProcessor(const SystemConfig& cfg) : config(cfg) {}

void SignalProcessor::process_signal_analysis(const ProcessedData& processed_data) {
    SignalDecision signal_decision = detect_trading_signals(processed_data, config);

    // Log candle data and enhanced signals table
    TradingLogs::log_candle_data_table(processed_data.curr.o, processed_data.curr.h, processed_data.curr.l, processed_data.curr.c);
    TradingLogs::log_signals_table_enhanced(signal_decision);

    // Enhanced detailed signal analysis logging
    TradingLogs::log_signal_analysis_detailed(processed_data, signal_decision, config);

    FilterResult filter_result = evaluate_trading_filters(processed_data, config);
    TradingLogs::log_filters(filter_result, config, processed_data);
    TradingLogs::log_summary(processed_data, signal_decision, filter_result, config.strategy.symbol);

    // Log signal analysis results
    log_signal_analysis_results(processed_data, signal_decision, filter_result);
}

std::pair<PositionSizing, SignalDecision> SignalProcessor::process_position_sizing(const ProcessedData& processed_data, double account_equity, int current_quantity, double buying_power) {
    PositionSizing sizing = calculate_position_sizing(processed_data, account_equity, current_quantity, config, buying_power);

    if (!evaluate_trading_filters(processed_data, config).all_pass) {
        TradingLogs::log_filters_not_met_preview(sizing.risk_amount, sizing.quantity);

        // CSV logging for position sizing when filters not met
        try {
            std::string timestamp = TimeUtils::get_current_human_readable_time();
            if (config.trading_mode.primary_symbol.empty()) {
                throw std::runtime_error("Primary symbol is required but not configured");
            }
            std::string symbol = config.trading_mode.primary_symbol;

            if (AlpacaTrader::Logging::g_csv_trade_logger) {
                AlpacaTrader::Logging::g_csv_trade_logger->log_position_sizing(
                    timestamp, symbol, sizing.quantity, sizing.risk_amount,
                    sizing.quantity * processed_data.curr.c, buying_power
                );
            }
        } catch (const std::exception& e) {
            TradingLogs::log_market_data_result_table("CSV logging error in position sizing: " + std::string(e.what()), false, 0);
        } catch (...) {
            TradingLogs::log_market_data_result_table("Unknown CSV logging error in position sizing", false, 0);
        }
        return {sizing, SignalDecision{}};
    }
    
    TradingLogs::log_filters_passed();
    TradingLogs::log_current_position(current_quantity, config.strategy.symbol);
    TradingLogs::log_position_size_with_buying_power(sizing.risk_amount, sizing.quantity, buying_power, processed_data.curr.c);
    TradingLogs::log_position_sizing_debug(sizing.risk_based_qty, sizing.exposure_based_qty, sizing.max_value_qty, sizing.buying_power_qty, sizing.quantity);

    SignalDecision signal_decision = detect_trading_signals(processed_data, config);

    // CSV logging for successful position sizing
    try {
        std::string timestamp = TimeUtils::get_current_human_readable_time();
        if (config.trading_mode.primary_symbol.empty()) {
            throw std::runtime_error("Primary symbol is required but not configured");
        }
        std::string symbol = config.trading_mode.primary_symbol;

        if (AlpacaTrader::Logging::g_csv_trade_logger) {
            AlpacaTrader::Logging::g_csv_trade_logger->log_position_sizing(
                timestamp, symbol, sizing.quantity, sizing.risk_amount,
                sizing.quantity * processed_data.curr.c, buying_power
            );
        }
    } catch (const std::exception& e) {
        TradingLogs::log_market_data_result_table("CSV logging error in successful position sizing: " + std::string(e.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown CSV logging error in successful position sizing", false, 0);
    }

    return {sizing, signal_decision};
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
