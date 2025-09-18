#ifndef TRADING_LOGGER_HPP
#define TRADING_LOGGER_HPP

#include "../configs/trader_config.hpp"
#include "../data/data_structures.hpp"
#include "../core/strategy_logic.hpp"
#include "logging_macros.hpp"

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
    static void log_summary(const ProcessedData& data, const StrategyLogic::SignalDecision& signals, const StrategyLogic::FilterResult& filters);
    static void log_filters_not_met_preview(double risk_amount, int quantity);
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
    
    // Performance metrics
    static void log_execution_time(const std::string& operation, long microseconds);
    static void log_system_health(const std::string& component, bool healthy, const std::string& details = "");
    
private:
    static std::string format_currency(double amount);
    static std::string format_percentage(double percentage);
};

#endif // TRADING_LOGGER_HPP
