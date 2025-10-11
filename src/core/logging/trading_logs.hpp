#ifndef TRADING_LOGS_HPP
#define TRADING_LOGS_HPP

#include "configs/system_config.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/trader/analysis/strategy_logic.hpp"
#include "logging_macros.hpp"
#include <vector>

using AlpacaTrader::Config::SystemConfig;
#include <tuple>
#include <string>

namespace AlpacaTrader {
namespace Logging {

/**
 * Specialized high-performance logging for trading operations.
 * Optimized for minimal latency in critical trading paths.
 */
class TradingLogs {
public:
    // Application lifecycle
    static void log_startup(const SystemConfig& config, double initial_equity);
    static void log_shutdown(unsigned long total_loops, double final_equity);
    
    // Trading loop events
    static void log_loop_header(unsigned long loop_number, const std::string& symbol);
    
    // Detailed trading analysis  
    static void log_candle_and_signals(const AlpacaTrader::Core::ProcessedData& data, const AlpacaTrader::Core::StrategyLogic::SignalDecision& signals);
    static void log_filters(const AlpacaTrader::Core::StrategyLogic::FilterResult& filters, const SystemConfig& config, const AlpacaTrader::Core::ProcessedData& data);
    static void log_summary(const AlpacaTrader::Core::ProcessedData& data, const AlpacaTrader::Core::StrategyLogic::SignalDecision& signals, const AlpacaTrader::Core::StrategyLogic::FilterResult& filters, const std::string& symbol = "");
    
    // Enhanced signal analysis logging
    static void log_signal_analysis_detailed(const AlpacaTrader::Core::ProcessedData& data, const AlpacaTrader::Core::StrategyLogic::SignalDecision& signals, const SystemConfig& config);
    static void log_momentum_analysis(const AlpacaTrader::Core::ProcessedData& data, const SystemConfig& config);
    static void log_signal_strength_breakdown(const AlpacaTrader::Core::StrategyLogic::SignalDecision& signals, const SystemConfig& config);
    static void log_signals_table_enhanced(const AlpacaTrader::Core::StrategyLogic::SignalDecision& signals);
    static void log_filters_not_met_preview(double risk_amount, int quantity);
    static void log_filters_not_met_table(double risk_amount, int quantity);
    static void log_position_size(double risk_amount, int quantity);
    static void log_position_size_with_buying_power(double risk_amount, int quantity, double buying_power, double current_price);
    static void log_current_position(int quantity, const std::string& symbol);
    static void log_signal_analysis_start(const std::string& symbol);
    static void log_signal_analysis_complete();
    
    
    // Market conditions
    static void log_market_status(bool is_open, const std::string& reason = "");
    static void log_trading_conditions(double daily_pnl, double exposure_pct, bool allowed, const SystemConfig& config);
    static void log_equity_update(double current_equity);
    
    // Signal processing
    static void log_market_data_status(bool has_data, size_t data_points = 0);
    static void log_signal_triggered(const std::string& signal_type, bool triggered);
    static void log_filters_passed();
    static void log_position_closure(const std::string& reason, int quantity);
    static void log_position_limits_reached(const std::string& side);
    static void log_no_trading_pattern();
    
    // Order management
    static void log_order_intent(const std::string& side, double entry_price, double stop_loss, double take_profit);
    static void log_order_result(const std::string& order_id, bool success, const std::string& reason = "");
    
    // Consolidated order execution logging
    static void log_comprehensive_order_execution(const std::string& order_type, const std::string& side, int quantity, 
                                                double current_price, double atr, int position_qty, double risk_amount,
                                                double stop_loss = 0.0, double take_profit = 0.0, 
                                                const std::string& symbol = "", const std::string& function_name = "");
    static void log_comprehensive_api_response(const std::string& order_id, const std::string& status, 
                                             const std::string& side, const std::string& quantity, 
                                             const std::string& order_class, const std::string& position_intent,
                                             const std::string& created_at, const std::string& filled_at,
                                             const std::string& filled_qty, const std::string& filled_avg_price,
                                             const std::string& error_code = "", const std::string& error_message = "",
                                             const std::string& available_qty = "", const std::string& existing_qty = "",
                                             const std::string& held_for_orders = "", const std::string& related_orders = "");
    
    // Order cancellation
    static void log_cancellation_start(const std::string& strategy, const std::string& signal_side = "");
    static void log_orders_found(int count, const std::string& symbol);
    static void log_orders_filtered(int count, const std::string& reason);
    static void log_cancellation_complete(int cancelled_count, const std::string& symbol);
    static void log_no_orders_to_cancel();
    
    // Position management
    static void log_position_closure_start(int quantity);
    static void log_fresh_position_data(int quantity);
    static void log_position_already_closed();
    static void log_closure_order_submitted(const std::string& side, int quantity);
    static void log_position_verification(int final_quantity);
    
    // Debug and validation logging
    static void log_trade_validation_failed(const std::string& reason);
    static void log_insufficient_buying_power(double required_buying_power, double available_buying_power, int quantity, double current_price);
    static void log_position_sizing_skipped(const std::string& reason);
    static void log_debug_position_data(int current_qty, double position_value, int position_qty, bool is_long, bool is_short);
    static void log_realtime_price_used(double realtime_price, double delayed_price);
    static void log_realtime_price_fallback(double delayed_price);
    static void log_debug_fresh_data_fetch(const std::string& position_type);
    static void log_debug_fresh_position_data(int fresh_qty, int current_qty);
    static void log_debug_account_details(int qty, double current_value);
    static void log_debug_position_closure_attempt(int qty);
    static void log_debug_position_closure_attempted();
    static void log_debug_position_verification(int verify_qty);
    static void log_debug_position_still_exists(const std::string& side);
    static void log_debug_no_position_found(const std::string& side);
    static void log_debug_skipping_trading_cycle();
    static void log_market_order_intent(const std::string& side, int quantity);
    
    // Market close position management
    static void log_market_close_warning(int minutes_until_close);
    static void log_market_close_position_closure(int quantity, const std::string& symbol, const std::string& side);
    static void log_market_close_complete();
    
    // Enhanced tabulated logging functions
    static void log_position_sizing_table(double risk_amount, int quantity, double buying_power, double current_price);
    static void log_sizing_analysis_table(int risk_based_qty, int exposure_based_qty, int max_value_qty, int buying_power_qty, int final_qty);
    static void log_position_sizing_debug(int risk_based_qty, int exposure_based_qty, int max_value_qty, int buying_power_qty, int final_qty);
    static void log_exit_targets_table(const std::string& side, double price, double risk, double rr, double stop_loss, double take_profit);
    static void log_order_result_table(const std::string& operation, const std::string& response);
    
    // Trading decision tables
    static void log_trading_conditions_table(double daily_pnl_pct, double daily_loss_limit, double daily_profit_target, double exposure_pct, double max_exposure_pct, bool conditions_met);
    static void log_candle_data_table(double open, double high, double low, double close);
    static void log_signals_table(bool buy_signal, bool sell_signal);
    static void log_filters_table(bool atr_pass, double atr_value, double atr_threshold, bool volume_pass, double volume_ratio, double volume_threshold, bool doji_pass);
    static void log_decision_summary_table(const std::string& symbol, double price, bool buy_signal, bool sell_signal, bool atr_pass, bool volume_pass, bool doji_pass, double exposure_pct, double atr_ratio, double volume_ratio);
    
    // System startup and status tables
    static void log_trader_startup_table(const SystemConfig& config, double initial_equity, double risk_per_trade, double rr_ratio, int loop_interval);
    static void log_account_overview_table(const std::string& account_number, const std::string& status, const std::string& currency, bool pattern_day_trader, const std::string& created_date);
    static void log_financial_summary_table(double equity, double last_equity, double cash, double buying_power, double long_market_value, double short_market_value, double initial_margin, double maintenance_margin, double sma, int day_trade_count, double regt_buying_power, double day_trading_buying_power);
    static void log_current_positions_table(int quantity, double current_value, double unrealized_pnl, double exposure_pct, int open_orders);
    static void log_data_source_table(const std::string& symbol, const std::string& account_type);
    static void log_thread_system_table(bool priorities_enabled, bool cpu_affinity_enabled);
    static void log_thread_priorities_table(const std::vector<std::tuple<std::string, std::string, bool>>& thread_statuses = {});
    static void log_data_source_info_table(const std::string& source, double price, const std::string& status);
    static void log_runtime_config_table(const AlpacaTrader::Config::SystemConfig& config);
    static void log_strategy_config_table(const AlpacaTrader::Config::SystemConfig& config);
    
    // Market data fetching tables
    static void log_market_data_fetch_table();
    static void log_market_data_result_table(const std::string& description, bool success, size_t bar_count);
    
    // Performance metrics
    
    // Inline status and countdown logging
    static void log_inline_halt_status(int seconds);
    static void log_inline_next_loop(int seconds);
    static void end_inline_status();
    
    // Order execution header
    static void log_order_execution_header();
    
private:
    static std::string format_currency(double amount);
    static std::string format_percentage(double percentage);
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // TRADING_LOGS_HPP
