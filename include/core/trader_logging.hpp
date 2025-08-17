#ifndef TRADER_LOGGING_HPP
#define TRADER_LOGGING_HPP

#include "../configs/trader_config.hpp"
#include "../data/data_structures.hpp"
#include "strategy_logic.hpp"

namespace TraderLogging {

// App header and config
void log_header_and_config(const TraderConfig& cfg);
void log_trader_started(const TraderConfig& cfg, double initial_equity);

// Loop visuals
void log_loop_header(unsigned long loop_number, const TraderConfig& cfg);
void log_halted_header(const TraderConfig& cfg);
void log_equity_status(double equity, const TraderConfig& cfg);

// Trading conditions
void log_trading_conditions_start(const TraderConfig& cfg);
void log_market_closed(const TraderConfig& cfg);
void log_daily_pnl_line(double daily_pnl, const TraderConfig& cfg);
void log_pnl_limit_reached(const TraderConfig& cfg);
void log_exposure_line(double exposure_pct, const TraderConfig& cfg);
void log_exposure_limit_reached(const TraderConfig& cfg);
void log_trading_allowed(const TraderConfig& cfg);
void log_trading_halted_tail(const TraderConfig& cfg);

// Market data collection
void log_market_data_header(const TraderConfig& cfg);
void log_no_market_data(const TraderConfig& cfg);
void log_insufficient_data(size_t got, int needed, const TraderConfig& cfg);
void log_indicator_failure(const TraderConfig& cfg);
void log_position_market_summary(const ProcessedData& data, const TraderConfig& cfg);
void log_computing_indicators_start(const TraderConfig& cfg);
void log_getting_position_and_account(const TraderConfig& cfg);
void log_market_data_collection_failed(const TraderConfig& cfg);
void log_missing_bracket_warning(const TraderConfig& cfg);
void log_computing_indicators_start(const TraderConfig& cfg);
void log_getting_position_and_account(const TraderConfig& cfg);
void log_market_data_collection_failed(const TraderConfig& cfg);
void log_missing_bracket_warning(const TraderConfig& cfg);

// Signal analysis
void log_signal_analysis_start(const TraderConfig& cfg);
void log_candle_and_signals(const ProcessedData& data, const StrategyLogic::SignalDecision& sd, const TraderConfig& cfg);
void log_filters(const StrategyLogic::FilterResult& fr, const TraderConfig& cfg);
void log_summary(const ProcessedData& data, const StrategyLogic::SignalDecision& sd, const StrategyLogic::FilterResult& fr, const TraderConfig& cfg);
void log_filters_not_met_preview(double risk_prev, int qty_prev, const TraderConfig& cfg);
void log_filters_pass(const TraderConfig& cfg);
void log_current_position(int current_qty, const TraderConfig& cfg);
void log_position_size(double risk_amount, int qty, const TraderConfig& cfg);
void log_qty_too_small(const TraderConfig& cfg);
void log_buy_triggered(const TraderConfig& cfg);
void log_sell_triggered(const TraderConfig& cfg);
void log_close_position_first(const char* side, const TraderConfig& cfg);
void log_open_position_details(const char* label, double entry, double sl, double tp, const TraderConfig& cfg);
void log_position_limits_reached(const char* side, const TraderConfig& cfg);
void log_no_valid_pattern(const TraderConfig& cfg);
void log_signal_analysis_complete(const TraderConfig& cfg);

} // namespace TraderLogging

#endif // TRADER_LOGGING_HPP


