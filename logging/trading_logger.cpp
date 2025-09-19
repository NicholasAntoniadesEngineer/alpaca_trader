#include "trading_logger.hpp"
#include "startup_logger.hpp"
#include "async_logger.hpp"
#include "logging_macros.hpp"
#include <iomanip>
#include <sstream>
#include <climits>


std::string TradingLogger::format_currency(double amount) {
    std::ostringstream oss;
    oss << "$" << std::fixed << std::setprecision(2) << amount;
    return oss.str();
}

std::string TradingLogger::format_percentage(double percentage) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << percentage << "%";
    return oss.str();
}

void TradingLogger::log_startup(const TraderConfig& config, double initial_equity) {
    log_trader_startup_table(
        config.target.symbol,
        initial_equity,
        config.risk.risk_per_trade,
        config.strategy.rr_ratio
    );
}

void TradingLogger::log_shutdown(unsigned long total_loops, double final_equity) {
    
    log_message("Trading session complete", "");
    log_message("Total loops executed: " + std::to_string(total_loops), "");
    log_message("Final equity: " + format_currency(final_equity), "");
}

void TradingLogger::log_loop_start(unsigned long loop_number) {
    log_message("Trading loop #" + std::to_string(loop_number) + " starting", "");
}

void TradingLogger::log_loop_complete() {
    log_message("Trading loop complete", "");
}

void TradingLogger::log_market_status(bool is_open, const std::string& reason) {
    if (is_open) {
        log_message("Market is OPEN - trading allowed", "");
    } else {
        std::string msg = "Market is CLOSED";
        if (!reason.empty()) {
            msg += " - " + reason;
        }
        log_message(msg, "");
    }
}

void TradingLogger::log_trading_conditions(double daily_pnl, double exposure_pct, bool allowed, const TraderConfig& config) {
    LOG_THREAD_TRADING_CONDITIONS_HEADER();
    log_trading_conditions_table(daily_pnl * 100.0, config.risk.daily_max_loss * 100.0, config.risk.daily_profit_target * 100.0, exposure_pct, config.risk.max_exposure_pct, allowed);
}

void TradingLogger::log_equity_update(double current_equity) {
    LOG_THREAD_SECTION_HEADER("EQUITY UPDATE");
    std::ostringstream oss;
    oss << "Current Equity: " << format_currency(current_equity) << " (acct poll=5s)";
    LOG_THREAD_CONTENT(oss.str());
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogger::log_market_data_status(bool has_data, size_t data_points) {
    
    if (has_data) {
        log_message("Market data available (" + std::to_string(data_points) + " points)", "");
    } else {
        log_message("No market data available", "");
    }
}

void TradingLogger::log_signal_analysis(const std::string& symbol, bool buy_signal, bool sell_signal) {
    
    std::ostringstream oss;
    oss << symbol << " signals - BUY: " << (buy_signal ? "YES" : "NO") 
        << " | SELL: " << (sell_signal ? "YES" : "NO");
    
    log_message(oss.str(), "");
}

void TradingLogger::log_filter_results(bool atr_pass, bool volume_pass, bool doji_pass) {
    
    std::ostringstream oss;
    oss << "Filters - ATR: " << (atr_pass ? "PASS" : "FAIL")
        << " | VOL: " << (volume_pass ? "PASS" : "FAIL")
        << " | DOJI: " << (doji_pass ? "PASS" : "FAIL");
    
    log_message(oss.str(), "");
}

void TradingLogger::log_position_sizing(double risk_amount, int quantity) {
    
    std::ostringstream oss;
    oss << "Position sizing - Risk: " << format_currency(risk_amount) 
        << " | Qty: " << quantity;
    
    log_message(oss.str(), "");
}

void TradingLogger::log_order_intent(const std::string& side, double entry_price, double stop_loss, double take_profit) {
    
    std::ostringstream oss;
    oss << side << " order intent - Entry: " << format_currency(entry_price)
        << " | SL: " << format_currency(stop_loss)
        << " | TP: " << format_currency(take_profit);
    
    log_message("ORDER", oss.str());
}

void TradingLogger::log_order_result(const std::string& order_id, bool success, const std::string& reason) {
    
    std::ostringstream oss;
    oss << "Order " << order_id << " - " << (success ? "SUCCESS" : "FAILED");
    if (!reason.empty()) {
        oss << " (" << reason << ")";
    }
    
    if (success) {
        log_message("ORDER", oss.str());
    } else {
        log_message("ORDER", oss.str());
    }
}

void TradingLogger::log_position_update(int current_quantity, double unrealized_pnl) {
    
    std::ostringstream oss;
    oss << "Position: " << current_quantity << " shares";
    if (unrealized_pnl != 0.0) {
        oss << " | Unrealized P/L: " << format_currency(unrealized_pnl);
    }
    
    log_message("POSITION", oss.str());
}

void TradingLogger::log_execution_time(const std::string& operation, long microseconds) {
    
    std::ostringstream oss;
    oss << operation << " execution time: " << microseconds << "μs";
    
    log_message("PERF", oss.str());
}

void TradingLogger::log_system_health(const std::string& component, bool healthy, const std::string& details) {
    
    std::ostringstream oss;
    oss << component << " health: " << (healthy ? "OK" : "ERROR");
    if (!details.empty()) {
        oss << " - " << details;
    }
    
    if (healthy) {
        log_message(oss.str(), "");
    } else {
        log_message(oss.str(), "");
    }
}

// Detailed trading analysis logging

void TradingLogger::log_loop_header(unsigned long loop_number, const std::string& symbol) {
    LOG_TRADING_LOOP_HEADER(loop_number, symbol);
}

void TradingLogger::log_header_and_config(const TraderConfig& config) {
    StartupLogger::log_trading_configuration(config);
}

void TradingLogger::log_candle_and_signals(const ProcessedData& data, const StrategyLogic::SignalDecision& signals) {
    log_candle_data_table(data.curr.o, data.curr.h, data.curr.l, data.curr.c);
    log_signals_table(signals.buy, signals.sell);
}

void TradingLogger::log_filters(const StrategyLogic::FilterResult& filters, const TraderConfig& config) {
    log_filters_table(filters.atr_pass, filters.atr_ratio, config.strategy.atr_multiplier_entry, 
                     filters.vol_pass, filters.vol_ratio, config.strategy.volume_multiplier, 
                     filters.doji_pass);
}

void TradingLogger::log_summary(const ProcessedData& data, const StrategyLogic::SignalDecision& signals, const StrategyLogic::FilterResult& filters, const std::string& symbol) {
    std::string display_symbol = symbol.empty() ? "SPY" : symbol;
    log_decision_summary_table(display_symbol, data.curr.c, signals.buy, signals.sell, 
                              filters.atr_pass, filters.vol_pass, filters.doji_pass, 
                              data.exposure_pct, filters.atr_ratio, filters.vol_ratio);
}

void TradingLogger::log_signal_analysis_start(const std::string& symbol) {
    LOG_THREAD_SIGNAL_ANALYSIS_HEADER(symbol);
    LOG_THREAD_SEPARATOR();
}

void TradingLogger::log_signal_analysis_complete() {
    LOG_THREAD_SEPARATOR();
    LOG_SIGNAL_ANALYSIS_COMPLETE();
    log_message("", "");
}

void TradingLogger::log_filters_not_met_preview(double risk_amount, int quantity) {
    LOG_THREAD_SEPARATOR();
    LOG_FILTERS_FAILED_HEADER();
    LOG_THREAD_CONTENT("Position would have been:");
    
    std::ostringstream risk_oss;
    risk_oss << "- Risk Amount: " << format_currency(risk_amount) << "/share";
    LOG_THREAD_SUBCONTENT(risk_oss.str());
    
    std::ostringstream qty_oss;
    qty_oss << "- Quantity: " << quantity << " shares";
    LOG_THREAD_SUBCONTENT(qty_oss.str());
    
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogger::log_position_size(double risk_amount, int quantity) {
    std::ostringstream oss;
    oss << "Position sizing - Risk: " << format_currency(risk_amount) << " | Qty: " << quantity;
    log_message(oss.str(), "");
}

void TradingLogger::log_position_size_with_buying_power(double risk_amount, int quantity, double buying_power, double current_price) {
    LOG_THREAD_POSITION_SIZING_HEADER();
    log_position_sizing_table(risk_amount, quantity, buying_power, current_price);
}

void TradingLogger::log_position_sizing_debug(int risk_based_qty, int exposure_based_qty, int max_value_qty, int buying_power_qty, int final_qty) {
    log_sizing_analysis_table(risk_based_qty, exposure_based_qty, max_value_qty, buying_power_qty, final_qty);
}

void TradingLogger::log_current_position(int quantity, const std::string& symbol) {
    LOG_THREAD_CURRENT_POSITION_HEADER();
    std::ostringstream oss;
    if (quantity == 0) {
        oss << "No position in " << symbol;
    } else if (quantity > 0) {
        oss << "LONG " << quantity << " shares of " << symbol;
    } else {
        oss << "SHORT " << (-quantity) << " shares of " << symbol;
    }
    LOG_THREAD_CONTENT(oss.str());
    LOG_THREAD_SEPARATOR();
}

// ========================================================================
// Enhanced Tabulated Logging Functions
// ========================================================================

void TradingLogger::log_position_sizing_table(double risk_amount, int quantity, double buying_power, double current_price) {
    double position_value = quantity * current_price;
    
    TABLE_HEADER_30("Parameter", "Value");
    TABLE_ROW_30("Risk Amount", format_currency(risk_amount));
    TABLE_ROW_30("Quantity", std::to_string(quantity) + " shares");
    TABLE_ROW_30("Position Value", format_currency(position_value));
    TABLE_ROW_30("Buying Power", format_currency(buying_power));
    TABLE_FOOTER_30();
    log_message("", "");
}

void TradingLogger::log_sizing_analysis_table(int risk_based_qty, int exposure_based_qty, int max_value_qty, int buying_power_qty, int final_qty) {
    TABLE_HEADER_30("Sizing Analysis", "Calculated Quantities");
    
    TABLE_ROW_30("Risk-Based", std::to_string(risk_based_qty) + " shares");
    TABLE_ROW_30("Exposure-Based", std::to_string(exposure_based_qty) + " shares");
    
    if (max_value_qty > 0) {
        TABLE_ROW_30("Max Value", std::to_string(max_value_qty) + " shares");
    }
    
    std::string bp_str = (buying_power_qty == INT_MAX) ? "unlimited" : (std::to_string(buying_power_qty) + " shares");
    TABLE_ROW_30("Buying Power", bp_str);
    
    TABLE_SEPARATOR_30();
    
    TABLE_ROW_30("FINAL QUANTITY", std::to_string(final_qty) + " shares");
    
    if (final_qty == 0) {
        std::string limitations = "";
        if (risk_based_qty == 0) limitations += "RISK ";
        if (exposure_based_qty == 0) limitations += "EXPOSURE ";
        if (max_value_qty == 0) limitations += "MAX_VALUE ";
        if (buying_power_qty == 0) limitations += "BUYING_POWER ";
        if (!limitations.empty()) {
            TABLE_ROW_30("LIMITED BY", limitations);
        }
    }
    
    TABLE_FOOTER_30();
}

void TradingLogger::log_exit_targets_table(const std::string& side, double price, double risk, double rr, double stop_loss, double take_profit) {
    TABLE_HEADER_30("Exit Targets", "Calculated Prices");
    
    TABLE_ROW_30("Order Side", side);
    TABLE_ROW_30("Entry Price", format_currency(price));
    TABLE_ROW_30("Risk Amount", format_currency(risk));
    TABLE_ROW_30("Risk/Reward", "1:" + std::to_string(rr));
    
    TABLE_SEPARATOR_30();
    
    TABLE_ROW_30("Stop Loss", format_currency(stop_loss));
    TABLE_ROW_30("Take Profit", format_currency(take_profit));
    
    TABLE_FOOTER_30();
}

void TradingLogger::log_order_result_table(const std::string& operation, const std::string& response) {
    TABLE_HEADER_48("Order Result", "Details");
    
    // Parse operation for multiline display
    std::string op_line1 = operation;
    std::string op_line2 = "";
    
    // Check if operation contains bracket order details
    size_t tp_pos = operation.find("(TP:");
    if (tp_pos != std::string::npos) {
        op_line1 = operation.substr(0, tp_pos);
        op_line2 = operation.substr(tp_pos);
        // Remove trailing space from line 1
        if (!op_line1.empty() && op_line1.back() == ' ') {
            op_line1.pop_back();
        }
    }
    
    TABLE_ROW_48("Operation", op_line1);
    
    if (!op_line2.empty()) {
        TABLE_ROW_48("", op_line2);
    }
    
    std::string status = "Failed";
    std::string order_id = "";
    
    if (!response.empty()) {
        try {
            // Simple JSON parsing to extract order ID
            size_t id_pos = response.find("\"id\":");
            if (id_pos != std::string::npos) {
                size_t start = response.find("\"", id_pos + 5) + 1;
                size_t end = response.find("\"", start);
                if (start != std::string::npos && end != std::string::npos) {
                    status = "Success";
                    order_id = response.substr(start, end - start);
                }
            } else {
                status = "Unknown Response";
            }
        } catch (...) {
            status = "Parse Error";
        }
    }
    
    TABLE_ROW_48("Status", status);
    
    if (!order_id.empty()) {
        TABLE_ROW_48("Order ID", order_id);
    }
    
    TABLE_FOOTER_48();
}

void TradingLogger::log_data_source_info_table(const std::string& source, double price, const std::string& status) {
    TABLE_HEADER_48("Data Source", "Market Information");
    TABLE_ROW_48("Feed", source);
    TABLE_ROW_48("Price", format_currency(price));
    TABLE_ROW_48("Status", status);
    TABLE_FOOTER_48();
}

// ========================================================================
// System Startup and Status Tables
// ========================================================================

void TradingLogger::log_trader_startup_table(const std::string& symbol, double initial_equity, double risk_per_trade, double rr_ratio) {
    TABLE_HEADER_48("Trader Startup", "Configuration");
    
    TABLE_ROW_48("Trading Symbol", symbol);
    TABLE_ROW_48("Initial Equity", format_currency(initial_equity));
    
    std::string risk_display = std::to_string(risk_per_trade * 100.0).substr(0,5) + "%";
    TABLE_ROW_48("Risk per Trade", risk_display);
    
    std::string rr_display = "1:" + std::to_string(rr_ratio).substr(0,6);
    TABLE_ROW_48("Risk/Reward", rr_display);
    
    TABLE_FOOTER_48();
}

void TradingLogger::log_account_overview_table(const std::string& account_number, const std::string& status, const std::string& currency, bool pattern_day_trader, const std::string& created_date) {
    TABLE_HEADER_48("Account Overview", "Details");
    
    TABLE_ROW_48("Account Number", account_number);
    TABLE_ROW_48("Status", status);
    TABLE_ROW_48("Currency", currency);
    
    std::string pdt_display = pattern_day_trader ? "YES" : "NO";
    TABLE_ROW_48("Pattern Day Trader", pdt_display);
    TABLE_ROW_48("Created", created_date);
    
    TABLE_FOOTER_48();
}

void TradingLogger::log_financial_summary_table(double equity, double last_equity, double cash, double buying_power, double long_market_value, double short_market_value, double initial_margin, double maintenance_margin, double sma, int day_trade_count, double regt_buying_power, double day_trading_buying_power) {
    TABLE_HEADER_48("Financial Summary", "Account Values");
    
    TABLE_ROW_48("Equity", format_currency(equity));
    TABLE_ROW_48("Last Equity", format_currency(last_equity));
    TABLE_ROW_48("Cash", format_currency(cash));
    TABLE_ROW_48("Buying Power", format_currency(buying_power));
    TABLE_ROW_48("Long Market Val", format_currency(long_market_value));
    TABLE_ROW_48("Short Market Val", format_currency(short_market_value));
    TABLE_ROW_48("Initial Margin", format_currency(initial_margin));
    TABLE_ROW_48("Maint Margin", format_currency(maintenance_margin));
    TABLE_ROW_48("SMA", format_currency(sma));
    TABLE_ROW_48("Day Trade Count", std::to_string(day_trade_count));
    TABLE_ROW_48("RegT Buying Power", format_currency(regt_buying_power));
    TABLE_ROW_48("DT Buying Power", format_currency(day_trading_buying_power));
    
    TABLE_FOOTER_48();
}

void TradingLogger::log_current_positions_table(int quantity, double current_value, double unrealized_pnl, double exposure_pct, int open_orders) {
    TABLE_HEADER_48("Current Position", "Portfolio Status");
    
    std::string position_display;
    if (quantity == 0) {
        position_display = "No position";
    } else if (quantity > 0) {
        position_display = "LONG " + std::to_string(quantity) + " shares";
    } else {
        position_display = "SHORT " + std::to_string(-quantity) + " shares";
    }
    TABLE_ROW_48("Position", position_display);
    TABLE_ROW_48("Current Value", format_currency(current_value));
    TABLE_ROW_48("Unrealized P/L", format_currency(unrealized_pnl));
    
    std::string exp_display = std::to_string(exposure_pct).substr(0,4) + "%";
    TABLE_ROW_48("Exposure", exp_display);
    TABLE_ROW_48("Open Orders", std::to_string(open_orders));
    
    TABLE_FOOTER_48();
}

void TradingLogger::log_data_source_table(const std::string& symbol, const std::string& account_type) {
    TABLE_HEADER_48("Data Sources", "Configuration");
    
    TABLE_ROW_48("Historical Bars", "IEX Feed (15-min delayed)");
    TABLE_ROW_48("Real-time Quotes", "IEX Free (limited coverage)");
    TABLE_ROW_48("Trading Symbol", symbol);
    TABLE_ROW_48("Account Type", account_type);
    
    TABLE_FOOTER_48();
}

void TradingLogger::log_configuration_table(const std::string& symbol, double risk_per_trade, double rr_ratio, int loop_interval) {
    TABLE_HEADER_48("Configuration", "Trading Parameters");
    
    TABLE_ROW_48("Symbol", symbol);
    
    std::string risk_display = std::to_string(risk_per_trade * 100.0).substr(0,5) + "%";
    TABLE_ROW_48("Risk per Trade", risk_display);
    
    std::string rr_display = "1:" + std::to_string(rr_ratio).substr(0,6);
    TABLE_ROW_48("Risk/Reward", rr_display);
    
    std::string interval_display = std::to_string(loop_interval) + " seconds";
    TABLE_ROW_48("Loop Interval", interval_display);
    
    TABLE_FOOTER_48();
}

void TradingLogger::log_thread_system_table(bool priorities_enabled, bool cpu_affinity_enabled) {
    TABLE_HEADER_48("Thread System", "Configuration");
    
    std::string priorities_display = priorities_enabled ? "ENABLED" : "DISABLED";
    TABLE_ROW_48("Thread Priorities", priorities_display);
    
    std::string affinity_display = cpu_affinity_enabled ? "ENABLED" : "DISABLED";
    TABLE_ROW_48("CPU Affinity", affinity_display);
    
    TABLE_FOOTER_48();
}

void TradingLogger::log_thread_priorities_table(const std::vector<std::tuple<std::string, std::string, bool>>& thread_statuses) {
    TABLE_HEADER_48("Thread Priorities", "Status");
    
    if (thread_statuses.empty()) {
        // Default fallback if no status provided
        TABLE_ROW_48("TRADER", "HIGHEST priority [OK]");
        TABLE_ROW_48("MARKET", "HIGH priority [OK]");
        TABLE_ROW_48("ACCOUNT", "NORMAL priority [OK]");
        TABLE_ROW_48("GATE", "LOW priority [OK]");
        TABLE_ROW_48("LOGGER", "LOWEST priority [OK]");
    } else {
        for (const auto& status : thread_statuses) {
            std::string thread_name = std::get<0>(status);
            std::string priority = std::get<1>(status);
            bool success = std::get<2>(status);
            
            std::string status_display = priority + " priority [" + (success ? "OK" : "FAIL") + "]";
            TABLE_ROW_48(thread_name, status_display);
        }
    }
    
    TABLE_FOOTER_48();
}

// ========================================================================
// Trading Decision Tables
// ========================================================================

void TradingLogger::log_trading_conditions_table(double daily_pnl_pct, double daily_loss_limit, double daily_profit_target, double exposure_pct, double max_exposure_pct, bool conditions_met) {
    TABLE_HEADER_48("Trading Conditions", "Current Values");
    
    std::ostringstream pnl_oss;
    pnl_oss << std::fixed << std::setprecision(3) << daily_pnl_pct << "%";
    std::string pnl_limits = "(" + std::to_string(daily_loss_limit).substr(0,6) + "% to " + std::to_string(daily_profit_target).substr(0,5) + "%)";
    std::string pnl_display = pnl_oss.str() + " " + pnl_limits;
    
    TABLE_ROW_48("Daily P/L", pnl_display);
    
    std::string exp_display = std::to_string((int)exposure_pct) + "% (Max: " + std::to_string((int)max_exposure_pct) + "%)";
    TABLE_ROW_48("Exposure", exp_display);
    
    TABLE_SEPARATOR_48();
    
    std::string result = conditions_met ? "All conditions met - Trading allowed" : "Conditions not met - Trading blocked";
    TABLE_ROW_48("RESULT", result);
    
    TABLE_FOOTER_48();
}

void TradingLogger::log_candle_data_table(double open, double high, double low, double close) {
    TABLE_HEADER_48("Candle Data", "OHLC Values");
    TABLE_ROW_48("Open", format_currency(open));
    TABLE_ROW_48("High", format_currency(high));
    TABLE_ROW_48("Low", format_currency(low));
    TABLE_ROW_48("Close", format_currency(close));
    TABLE_FOOTER_48();
}

void TradingLogger::log_signals_table(bool buy_signal, bool sell_signal) {
    TABLE_HEADER_48("Signal Analysis", "Detection Results");
    TABLE_ROW_48("BUY Signal", (buy_signal ? "YES" : "NO"));
    TABLE_ROW_48("SELL Signal", (sell_signal ? "YES" : "NO"));
    TABLE_FOOTER_48();
}

void TradingLogger::log_filters_table(bool atr_pass, double atr_ratio, double atr_threshold, bool volume_pass, double volume_ratio, double volume_threshold, bool doji_pass) {
    TABLE_HEADER_48("Filter Analysis", "Validation Results");
    
    std::string atr_status = atr_pass ? "PASS" : "FAIL";
    std::string atr_detail = "(" + std::to_string(atr_ratio).substr(0,4) + "x > " + std::to_string(atr_threshold).substr(0,4) + "x)";
    std::string atr_display = atr_status + " " + atr_detail;
    TABLE_ROW_48("ATR Filter", atr_display);
    
    std::string vol_status = volume_pass ? "PASS" : "FAIL";
    std::string vol_detail = "(" + std::to_string(volume_ratio).substr(0,4) + "x > " + std::to_string(volume_threshold).substr(0,4) + "x)";
    std::string vol_display = vol_status + " " + vol_detail;
    TABLE_ROW_48("Volume Filter", vol_display);
    
    std::string doji_status = doji_pass ? "PASS" : "FAIL";
    TABLE_ROW_48("Doji Filter", doji_status);
    
    TABLE_FOOTER_48();
}

void TradingLogger::log_decision_summary_table(const std::string& symbol, double price, bool buy_signal, bool sell_signal, bool atr_pass, bool volume_pass, bool doji_pass, double exposure_pct, double atr_ratio, double volume_ratio) {
    TABLE_HEADER_48("Decision Summary", "Trading Analysis Results");
    
    std::string price_display = symbol + " @ " + format_currency(price);
    TABLE_ROW_48("Symbol & Price", price_display);
    
    std::string signals_display = "BUY=" + std::string(buy_signal ? "YES" : "NO") + "  SELL=" + std::string(sell_signal ? "YES" : "NO");
    TABLE_ROW_48("Signals", signals_display);
    
    std::string filters_display = "ATR=" + std::string(atr_pass ? "PASS" : "FAIL") + " VOL=" + std::string(volume_pass ? "PASS" : "FAIL") + " DOJI=" + std::string(doji_pass ? "PASS" : "FAIL");
    TABLE_ROW_48("Filters", filters_display);
    
    std::string exp_display = std::to_string((int)exposure_pct) + "%";
    TABLE_ROW_48("Exposure", exp_display);
    
    std::string ratios_display = "ATR=" + std::to_string(atr_ratio).substr(0,5) + "x  VOL=" + std::to_string(volume_ratio).substr(0,5) + "x";
    TABLE_ROW_48("Ratios", ratios_display);
    
    TABLE_FOOTER_48();
}

