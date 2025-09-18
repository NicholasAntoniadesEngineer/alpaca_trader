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
    
    std::ostringstream pnl_oss;
    pnl_oss << "Daily P/L: " << format_percentage(daily_pnl) 
            << " (Limits: " << format_percentage(config.risk.daily_max_loss) 
            << " to " << format_percentage(config.risk.daily_profit_target) << ")";
    LOG_THREAD_CONTENT(pnl_oss.str());
    
    std::ostringstream exp_oss;
    exp_oss << "Exposure: " << std::fixed << std::setprecision(0) << exposure_pct 
            << "% (Max: " << std::fixed << std::setprecision(0) << config.risk.max_exposure_pct << "%)";
    LOG_THREAD_CONTENT(exp_oss.str());
    
    LOG_THREAD_SEPARATOR();
    if (allowed) {
        LOG_THREAD_CONTENT("RESULT: All conditions met");
        LOG_THREAD_SECTION_HEADER("Trading allowed");
    } else {
        LOG_THREAD_CONTENT("RESULT: Trading halted");
        LOG_THREAD_SECTION_HEADER("Trading suspended");
    }
    log_message("", "");
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
    // Log candle OHLC data
    std::ostringstream oss;
    oss << "CANDLE DATA:  Open=" << std::fixed << std::setprecision(2) << data.curr.o 
        << "  High=" << data.curr.h 
        << "  Low=" << data.curr.l 
        << "  Close=" << data.curr.c;
    LOG_THREAD_CONTENT(oss.str());
    
    std::ostringstream signals_oss;
    signals_oss << "SIGNALS:      BUY=" << std::setw(3) << (signals.buy ? "YES" : "NO") 
                << "  SELL=" << std::setw(3) << (signals.sell ? "YES" : "NO");
    LOG_THREAD_CONTENT(signals_oss.str());
    LOG_THREAD_SEPARATOR();
}

void TradingLogger::log_filters(const StrategyLogic::FilterResult& filters, const TraderConfig& config) {
    // Create a more organized filter table
    LOG_THREAD_CONTENT("FILTERS:");
    
    std::ostringstream atr_oss;
    atr_oss << "ATR Filter:   " << std::setw(4) << (filters.atr_pass ? "PASS" : "FAIL") 
            << "  (" << std::fixed << std::setprecision(2) << filters.atr_ratio << "x > " 
            << config.strategy.atr_multiplier_entry << "x)";
    LOG_THREAD_SUBCONTENT(atr_oss.str());
    
    std::ostringstream vol_oss;
    vol_oss << "Volume Filter:" << std::setw(4) << (filters.vol_pass ? "PASS" : "FAIL") 
            << "  (" << std::fixed << std::setprecision(2) << filters.vol_ratio << "x > " 
            << config.strategy.volume_multiplier << "x)";
    LOG_THREAD_SUBCONTENT(vol_oss.str());
    
    std::ostringstream doji_oss;
    doji_oss << "Doji Filter:  " << std::setw(4) << (filters.doji_pass ? "PASS" : "FAIL");
    LOG_THREAD_SUBCONTENT(doji_oss.str());
    LOG_THREAD_SEPARATOR();
}

void TradingLogger::log_summary(const ProcessedData& data, const StrategyLogic::SignalDecision& signals, const StrategyLogic::FilterResult& filters) {
    LOG_THREAD_CONTENT("SUMMARY:");
    
    std::ostringstream summary_oss;
    summary_oss << "Price=" << std::fixed << std::setprecision(2) << data.curr.c 
                << "  Signals: BUY=" << (signals.buy ? "YES" : "NO") 
                << " SELL=" << (signals.sell ? "YES" : "NO");
    LOG_THREAD_SUBCONTENT(summary_oss.str());
    
    std::ostringstream filters_oss;
    filters_oss << "Filters: ATR=" << (filters.atr_pass ? "PASS" : "FAIL") 
                << " VOL=" << (filters.vol_pass ? "PASS" : "FAIL") 
                << " DOJI=" << (filters.doji_pass ? "PASS" : "FAIL") 
                << "  Exposure=" << std::fixed << std::setprecision(0) << data.exposure_pct << "%";
    LOG_THREAD_SUBCONTENT(filters_oss.str());
    
    std::ostringstream ratios_oss;
    ratios_oss << "Ratios: ATR=" << std::fixed << std::setprecision(3) << filters.atr_ratio 
               << "x  VOL=" << std::fixed << std::setprecision(3) << filters.vol_ratio << "x";
    LOG_THREAD_SUBCONTENT(ratios_oss.str());
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
    double position_value = quantity * current_price;
    LOG_THREAD_POSITION_SIZING_HEADER();
    
    std::ostringstream risk_oss;
    risk_oss << "Risk Amount:    " << format_currency(risk_amount);
    LOG_THREAD_CONTENT(risk_oss.str());
    
    std::ostringstream qty_oss;
    qty_oss << "Quantity:       " << quantity << " shares";
    LOG_THREAD_CONTENT(qty_oss.str());
    
    std::ostringstream value_oss;
    value_oss << "Position Value: " << format_currency(position_value);
    LOG_THREAD_CONTENT(value_oss.str());
    
    std::ostringstream bp_oss;
    bp_oss << "Buying Power:   " << format_currency(buying_power);
    LOG_THREAD_CONTENT(bp_oss.str());
    log_message("", "");
}

void TradingLogger::log_position_sizing_debug(int risk_based_qty, int exposure_based_qty, int buying_power_qty, int final_qty) {
    std::ostringstream debug_log;
    debug_log << "SIZING DEBUG: ";
    debug_log << "Risk=" << risk_based_qty << ", ";
    debug_log << "Exposure=" << exposure_based_qty << ", ";
    debug_log << "BuyPower=" << (buying_power_qty == INT_MAX ? "unlimited" : std::to_string(buying_power_qty)) << ", ";
    debug_log << "Final=" << final_qty << " shares";
    
    if (final_qty == 0) {
        debug_log << " [LIMITED BY: ";
        if (risk_based_qty == 0) debug_log << "RISK ";
        if (exposure_based_qty == 0) debug_log << "EXPOSURE ";
        if (buying_power_qty == 0) debug_log << "BUYING_POWER ";
        debug_log << "]";
    }
    
    LOG_THREAD_CONTENT(debug_log.str());
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

