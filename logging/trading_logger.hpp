#ifndef TRADING_LOGGER_HPP
#define TRADING_LOGGER_HPP

#include "../configs/trader_config.hpp"
#include "../data/data_structures.hpp"
#include "../core/strategy_logic.hpp"
#include "logging_macros.hpp"
#include <vector>
#include <tuple>

/**
 * Specialized high-performance logging for trading operations.
 * Optimized for minimal latency in critical trading paths.
 */
class TradingLogger {
public:
    // Application lifecycle
    static void log_startup(const TraderConfig& config, double initial_equity);
    static void log_shutdown(unsigned long total_loops, double final_equity);
    
    // Trading loop events
    static void log_loop_start(unsigned long loop_number);
    static void log_loop_complete();
    static void log_loop_header(unsigned long loop_number, const std::string& symbol);
    
    // Detailed trading analysis  
    static void log_candle_and_signals(const ProcessedData& data, const StrategyLogic::SignalDecision& signals);
    static void log_filters(const StrategyLogic::FilterResult& filters, const TraderConfig& config);
    static void log_summary(const ProcessedData& data, const StrategyLogic::SignalDecision& signals, const StrategyLogic::FilterResult& filters, const std::string& symbol = "");
    static void log_filters_not_met_preview(double risk_amount, int quantity);
    static void log_filters_not_met_table(double risk_amount, int quantity);
    static void log_position_size(double risk_amount, int quantity);
    static void log_position_size_with_buying_power(double risk_amount, int quantity, double buying_power, double current_price);
    static void log_position_sizing_debug(int risk_based_qty, int exposure_based_qty, int buying_power_qty, int final_qty);
    static void log_current_position(int quantity, const std::string& symbol);
    static void log_signal_analysis_start(const std::string& symbol);
    static void log_signal_analysis_complete();
    
    // Headers and configurations
    static void log_header_and_config(const TraderConfig& config);
    
    // Market conditions
    static void log_market_status(bool is_open, const std::string& reason = "");
    static void log_trading_conditions(double daily_pnl, double exposure_pct, bool allowed, const TraderConfig& config);
    static void log_equity_update(double current_equity);
    
    // Signal processing
    static void log_market_data_status(bool has_data, size_t data_points = 0);
    static void log_signal_analysis(const std::string& symbol, bool buy_signal, bool sell_signal);
    static void log_filter_results(bool atr_pass, bool volume_pass, bool doji_pass);
    static void log_position_sizing(double risk_amount, int quantity);
    
    // Order management
    static void log_order_intent(const std::string& side, double entry_price, double stop_loss, double take_profit);
    static void log_order_result(const std::string& order_id, bool success, const std::string& reason = "");
    static void log_position_update(int current_quantity, double unrealized_pnl = 0.0);
    
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
    static void log_filters_table(bool atr_pass, double atr_ratio, double atr_threshold, bool volume_pass, double volume_ratio, double volume_threshold, bool doji_pass);
    static void log_decision_summary_table(const std::string& symbol, double price, bool buy_signal, bool sell_signal, bool atr_pass, bool volume_pass, bool doji_pass, double exposure_pct, double atr_ratio, double volume_ratio);
    
    // System startup and status tables
    static void log_trader_startup_table(const std::string& symbol, double initial_equity, double risk_per_trade, double rr_ratio);
    static void log_account_overview_table(const std::string& account_number, const std::string& status, const std::string& currency, bool pattern_day_trader, const std::string& created_date);
    static void log_financial_summary_table(double equity, double last_equity, double cash, double buying_power, double long_market_value, double short_market_value, double initial_margin, double maintenance_margin, double sma, int day_trade_count, double regt_buying_power, double day_trading_buying_power);
    static void log_current_positions_table(int quantity, double current_value, double unrealized_pnl, double exposure_pct, int open_orders);
    static void log_data_source_table(const std::string& symbol, const std::string& account_type);
    static void log_configuration_table(const std::string& symbol, double risk_per_trade, double rr_ratio, int loop_interval);
    static void log_thread_system_table(bool priorities_enabled, bool cpu_affinity_enabled);
    static void log_thread_priorities_table(const std::vector<std::tuple<std::string, std::string, bool>>& thread_statuses = {});
    static void log_data_source_info_table(const std::string& source, double price, const std::string& status);
    
    // Market data fetching tables
    static void log_market_data_fetch_table(const std::string& symbol);
    static void log_market_data_attempt_table(const std::string& description);
    static void log_market_data_result_table(const std::string& description, bool success, size_t bar_count);
    
    // Performance metrics
    static void log_execution_time(const std::string& operation, long microseconds);
    static void log_system_health(const std::string& component, bool healthy, const std::string& details = "");
    
private:
    static std::string format_currency(double amount);
    static std::string format_percentage(double percentage);
};

#endif // TRADING_LOGGER_HPP
