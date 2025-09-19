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
    log_message("Advanced momentum trader starting for " + config.target.symbol, "");
    log_message("Initial equity: " + format_currency(initial_equity), "");
    log_message("Risk per trade: " + format_percentage(config.risk.risk_per_trade * 100), "");
    log_message("Risk/reward ratio: 1:" + std::to_string(config.strategy.rr_ratio), "");
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
    
    LOG_THREAD_CONTENT("┌─────────────────┬──────────────────────────────┐");
    LOG_THREAD_CONTENT("│ Parameter       │ Value                        │");
    LOG_THREAD_CONTENT("├─────────────────┼──────────────────────────────┤");
    
    std::ostringstream risk_oss;
    risk_oss << "│ Risk Amount     │ " << std::left << std::setw(28) << format_currency(risk_amount) << " │";
    LOG_THREAD_CONTENT(risk_oss.str());
    
    std::ostringstream qty_oss;
    qty_oss << "│ Quantity        │ " << std::left << std::setw(28) << (std::to_string(quantity) + " shares") << " │";
    LOG_THREAD_CONTENT(qty_oss.str());
    
    std::ostringstream value_oss;
    value_oss << "│ Position Value  │ " << std::left << std::setw(28) << format_currency(position_value) << " │";
    LOG_THREAD_CONTENT(value_oss.str());
    
    std::ostringstream bp_oss;
    bp_oss << "│ Buying Power    │ " << std::left << std::setw(28) << format_currency(buying_power) << " │";
    LOG_THREAD_CONTENT(bp_oss.str());
    
    LOG_THREAD_CONTENT("└─────────────────┴──────────────────────────────┘");
    log_message("", "");
}

void TradingLogger::log_sizing_analysis_table(int risk_based_qty, int exposure_based_qty, int max_value_qty, int buying_power_qty, int final_qty) {
    LOG_THREAD_CONTENT("┌─────────────────┬──────────────────────────────┐");
    LOG_THREAD_CONTENT("│ Sizing Analysis │ Calculated Quantities        │");
    LOG_THREAD_CONTENT("├─────────────────┼──────────────────────────────┤");
    
    std::ostringstream risk_oss;
    risk_oss << "│ Risk-Based      │ " << std::left << std::setw(28) << (std::to_string(risk_based_qty) + " shares") << " │";
    LOG_THREAD_CONTENT(risk_oss.str());
    
    std::ostringstream exposure_oss;
    exposure_oss << "│ Exposure-Based  │ " << std::left << std::setw(28) << (std::to_string(exposure_based_qty) + " shares") << " │";
    LOG_THREAD_CONTENT(exposure_oss.str());
    
    if (max_value_qty > 0) {
        std::ostringstream max_val_oss;
        max_val_oss << "│ Max Value       │ " << std::left << std::setw(28) << (std::to_string(max_value_qty) + " shares") << " │";
        LOG_THREAD_CONTENT(max_val_oss.str());
    }
    
    std::ostringstream bp_oss;
    std::string bp_str = (buying_power_qty == INT_MAX) ? "unlimited" : (std::to_string(buying_power_qty) + " shares");
    bp_oss << "│ Buying Power    │ " << std::left << std::setw(28) << bp_str << " │";
    LOG_THREAD_CONTENT(bp_oss.str());
    
    LOG_THREAD_CONTENT("├─────────────────┼──────────────────────────────┤");
    
    std::ostringstream final_oss;
    final_oss << "│ FINAL QUANTITY  │ " << std::left << std::setw(28) << (std::to_string(final_qty) + " shares") << " │";
    LOG_THREAD_CONTENT(final_oss.str());
    
    if (final_qty == 0) {
        std::ostringstream limit_oss;
        std::string limitations = "";
        if (risk_based_qty == 0) limitations += "RISK ";
        if (exposure_based_qty == 0) limitations += "EXPOSURE ";
        if (max_value_qty == 0) limitations += "MAX_VALUE ";
        if (buying_power_qty == 0) limitations += "BUYING_POWER ";
        if (!limitations.empty()) {
            limit_oss << "│ LIMITED BY      │ " << std::left << std::setw(28) << limitations << " │";
            LOG_THREAD_CONTENT(limit_oss.str());
        }
    }
    
    LOG_THREAD_CONTENT("└─────────────────┴──────────────────────────────┘");
}

void TradingLogger::log_exit_targets_table(const std::string& side, double price, double risk, double rr, double stop_loss, double take_profit) {
    LOG_THREAD_CONTENT("┌─────────────────┬──────────────────────────────┐");
    LOG_THREAD_CONTENT("│ Exit Targets    │ Calculated Prices            │");
    LOG_THREAD_CONTENT("├─────────────────┼──────────────────────────────┤");
    
    std::ostringstream side_oss;
    side_oss << "│ Order Side      │ " << std::left << std::setw(28) << side << " │";
    LOG_THREAD_CONTENT(side_oss.str());
    
    std::ostringstream entry_oss;
    entry_oss << "│ Entry Price     │ " << std::left << std::setw(28) << format_currency(price) << " │";
    LOG_THREAD_CONTENT(entry_oss.str());
    
    std::ostringstream risk_oss;
    risk_oss << "│ Risk Amount     │ " << std::left << std::setw(28) << format_currency(risk) << " │";
    LOG_THREAD_CONTENT(risk_oss.str());
    
    std::ostringstream rr_oss;
    rr_oss << "│ Risk/Reward     │ " << std::left << std::setw(28) << ("1:" + std::to_string(rr)) << " │";
    LOG_THREAD_CONTENT(rr_oss.str());
    
    LOG_THREAD_CONTENT("├─────────────────┼──────────────────────────────┤");
    
    std::ostringstream sl_oss;
    sl_oss << "│ Stop Loss       │ " << std::left << std::setw(28) << format_currency(stop_loss) << " │";
    LOG_THREAD_CONTENT(sl_oss.str());
    
    std::ostringstream tp_oss;
    tp_oss << "│ Take Profit     │ " << std::left << std::setw(28) << format_currency(take_profit) << " │";
    LOG_THREAD_CONTENT(tp_oss.str());
    
    LOG_THREAD_CONTENT("└─────────────────┴──────────────────────────────┘");
}

void TradingLogger::log_order_result_table(const std::string& operation, const std::string& response) {
    // Wider table to accommodate order IDs and long operations
    LOG_THREAD_CONTENT("┌─────────────────┬──────────────────────────────────────────────────┐");
    LOG_THREAD_CONTENT("│ Order Result    │ Details                                          │");
    LOG_THREAD_CONTENT("├─────────────────┼──────────────────────────────────────────────────┤");
    
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
    
    std::ostringstream op1_oss;
    op1_oss << "│ Operation       │ " << std::left << std::setw(48) << op_line1 << " │";
    LOG_THREAD_CONTENT(op1_oss.str());
    
    if (!op_line2.empty()) {
        std::ostringstream op2_oss;
        op2_oss << "│                 │ " << std::left << std::setw(48) << op_line2 << " │";
        LOG_THREAD_CONTENT(op2_oss.str());
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
    
    std::ostringstream status_oss;
    status_oss << "│ Status          │ " << std::left << std::setw(48) << status << " │";
    LOG_THREAD_CONTENT(status_oss.str());
    
    if (!order_id.empty()) {
        std::ostringstream id_oss;
        id_oss << "│ Order ID        │ " << std::left << std::setw(48) << order_id << " │";
        LOG_THREAD_CONTENT(id_oss.str());
    }
    
    LOG_THREAD_CONTENT("└─────────────────┴──────────────────────────────────────────────────┘");
}

// ========================================================================
// Trading Decision Tables
// ========================================================================

void TradingLogger::log_trading_conditions_table(double daily_pnl_pct, double daily_loss_limit, double daily_profit_target, double exposure_pct, double max_exposure_pct, bool conditions_met) {
    LOG_THREAD_CONTENT("┌─────────────────┬──────────────────────────────────────────────────┐");
    LOG_THREAD_CONTENT("│ Trading Conditions │ Current Values                                 │");
    LOG_THREAD_CONTENT("├─────────────────┼──────────────────────────────────────────────────┤");
    
    std::ostringstream pnl_oss;
    pnl_oss << std::fixed << std::setprecision(3) << daily_pnl_pct << "%";
    std::string pnl_limits = "(" + std::to_string(daily_loss_limit).substr(0,6) + "% to " + std::to_string(daily_profit_target).substr(0,5) + "%)";
    std::string pnl_display = pnl_oss.str() + " " + pnl_limits;
    
    std::ostringstream daily_oss;
    daily_oss << "│ Daily P/L       │ " << std::left << std::setw(48) << pnl_display << " │";
    LOG_THREAD_CONTENT(daily_oss.str());
    
    std::ostringstream exp_oss;
    std::string exp_display = std::to_string((int)exposure_pct) + "% (Max: " + std::to_string((int)max_exposure_pct) + "%)";
    exp_oss << "│ Exposure        │ " << std::left << std::setw(48) << exp_display << " │";
    LOG_THREAD_CONTENT(exp_oss.str());
    
    LOG_THREAD_CONTENT("├─────────────────┼──────────────────────────────────────────────────┤");
    
    std::ostringstream result_oss;
    std::string result = conditions_met ? "All conditions met - Trading allowed" : "Conditions not met - Trading blocked";
    result_oss << "│ RESULT          │ " << std::left << std::setw(48) << result << " │";
    LOG_THREAD_CONTENT(result_oss.str());
    
    LOG_THREAD_CONTENT("└─────────────────┴──────────────────────────────────────────────────┘");
}

void TradingLogger::log_candle_data_table(double open, double high, double low, double close) {
    LOG_THREAD_CONTENT("┌─────────────────┬──────────────────────────────────────────────────┐");
    LOG_THREAD_CONTENT("│ Candle Data     │ OHLC Values                                      │");
    LOG_THREAD_CONTENT("├─────────────────┼──────────────────────────────────────────────────┤");
    
    std::ostringstream open_oss;
    open_oss << "│ Open            │ " << std::left << std::setw(48) << format_currency(open) << " │";
    LOG_THREAD_CONTENT(open_oss.str());
    
    std::ostringstream high_oss;
    high_oss << "│ High            │ " << std::left << std::setw(48) << format_currency(high) << " │";
    LOG_THREAD_CONTENT(high_oss.str());
    
    std::ostringstream low_oss;
    low_oss << "│ Low             │ " << std::left << std::setw(48) << format_currency(low) << " │";
    LOG_THREAD_CONTENT(low_oss.str());
    
    std::ostringstream close_oss;
    close_oss << "│ Close           │ " << std::left << std::setw(48) << format_currency(close) << " │";
    LOG_THREAD_CONTENT(close_oss.str());
    
    LOG_THREAD_CONTENT("└─────────────────┴──────────────────────────────────────────────────┘");
}

void TradingLogger::log_signals_table(bool buy_signal, bool sell_signal) {
    LOG_THREAD_CONTENT("┌─────────────────┬──────────────────────────────────────────────────┐");
    LOG_THREAD_CONTENT("│ Signal Analysis │ Detection Results                                │");
    LOG_THREAD_CONTENT("├─────────────────┼──────────────────────────────────────────────────┤");
    
    std::ostringstream buy_oss;
    buy_oss << "│ BUY Signal      │ " << std::left << std::setw(48) << (buy_signal ? "YES" : "NO") << " │";
    LOG_THREAD_CONTENT(buy_oss.str());
    
    std::ostringstream sell_oss;
    sell_oss << "│ SELL Signal     │ " << std::left << std::setw(48) << (sell_signal ? "YES" : "NO") << " │";
    LOG_THREAD_CONTENT(sell_oss.str());
    
    LOG_THREAD_CONTENT("└─────────────────┴──────────────────────────────────────────────────┘");
}

void TradingLogger::log_filters_table(bool atr_pass, double atr_ratio, double atr_threshold, bool volume_pass, double volume_ratio, double volume_threshold, bool doji_pass) {
    LOG_THREAD_CONTENT("┌─────────────────┬──────────────────────────────────────────────────┐");
    LOG_THREAD_CONTENT("│ Filter Analysis │ Validation Results                               │");
    LOG_THREAD_CONTENT("├─────────────────┼──────────────────────────────────────────────────┤");
    
    std::ostringstream atr_oss;
    std::string atr_status = atr_pass ? "PASS" : "FAIL";
    std::string atr_detail = "(" + std::to_string(atr_ratio).substr(0,4) + "x > " + std::to_string(atr_threshold).substr(0,4) + "x)";
    std::string atr_display = atr_status + " " + atr_detail;
    atr_oss << "│ ATR Filter      │ " << std::left << std::setw(48) << atr_display << " │";
    LOG_THREAD_CONTENT(atr_oss.str());
    
    std::ostringstream vol_oss;
    std::string vol_status = volume_pass ? "PASS" : "FAIL";
    std::string vol_detail = "(" + std::to_string(volume_ratio).substr(0,4) + "x > " + std::to_string(volume_threshold).substr(0,4) + "x)";
    std::string vol_display = vol_status + " " + vol_detail;
    vol_oss << "│ Volume Filter   │ " << std::left << std::setw(48) << vol_display << " │";
    LOG_THREAD_CONTENT(vol_oss.str());
    
    std::ostringstream doji_oss;
    std::string doji_status = doji_pass ? "PASS" : "FAIL";
    doji_oss << "│ Doji Filter     │ " << std::left << std::setw(48) << doji_status << " │";
    LOG_THREAD_CONTENT(doji_oss.str());
    
    LOG_THREAD_CONTENT("└─────────────────┴──────────────────────────────────────────────────┘");
}

void TradingLogger::log_decision_summary_table(const std::string& symbol, double price, bool buy_signal, bool sell_signal, bool atr_pass, bool volume_pass, bool doji_pass, double exposure_pct, double atr_ratio, double volume_ratio) {
    LOG_THREAD_CONTENT("┌─────────────────┬──────────────────────────────────────────────────┐");
    LOG_THREAD_CONTENT("│ Decision Summary│ Trading Analysis Results                         │");
    LOG_THREAD_CONTENT("├─────────────────┼──────────────────────────────────────────────────┤");
    
    std::ostringstream symbol_oss;
    symbol_oss << "│ Symbol & Price  │ " << std::left << std::setw(48) << (symbol + " @ " + format_currency(price)) << " │";
    LOG_THREAD_CONTENT(symbol_oss.str());
    
    std::ostringstream signals_oss;
    std::string signals_display = "BUY=" + std::string(buy_signal ? "YES" : "NO") + "  SELL=" + std::string(sell_signal ? "YES" : "NO");
    signals_oss << "│ Signals         │ " << std::left << std::setw(48) << signals_display << " │";
    LOG_THREAD_CONTENT(signals_oss.str());
    
    std::ostringstream filters_oss;
    std::string filters_display = "ATR=" + std::string(atr_pass ? "PASS" : "FAIL") + " VOL=" + std::string(volume_pass ? "PASS" : "FAIL") + " DOJI=" + std::string(doji_pass ? "PASS" : "FAIL");
    filters_oss << "│ Filters         │ " << std::left << std::setw(48) << filters_display << " │";
    LOG_THREAD_CONTENT(filters_oss.str());
    
    std::ostringstream exposure_oss;
    std::string exp_display = std::to_string((int)exposure_pct) + "%";
    exposure_oss << "│ Exposure        │ " << std::left << std::setw(48) << exp_display << " │";
    LOG_THREAD_CONTENT(exposure_oss.str());
    
    std::ostringstream ratios_oss;
    std::string ratios_display = "ATR=" + std::to_string(atr_ratio).substr(0,5) + "x  VOL=" + std::to_string(volume_ratio).substr(0,5) + "x";
    ratios_oss << "│ Ratios          │ " << std::left << std::setw(48) << ratios_display << " │";
    LOG_THREAD_CONTENT(ratios_oss.str());
    
    LOG_THREAD_CONTENT("└─────────────────┴──────────────────────────────────────────────────┘");
}

