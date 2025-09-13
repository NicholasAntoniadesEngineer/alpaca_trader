#include "trading_logger.hpp"
#include "async_logger.hpp"
#include <iomanip>
#include <sstream>

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
    // Using unified log_message for consistent output
    
    log_message("Trading session complete", "");
    log_message("Total loops executed: " + std::to_string(total_loops), "");
    log_message("Final equity: " + format_currency(final_equity), "");
}

void TradingLogger::log_loop_start(unsigned long loop_number) {
    // Using unified log_message for consistent output
    log_message("Trading loop #" + std::to_string(loop_number) + " starting", "");
}

void TradingLogger::log_loop_complete() {
    // Using unified log_message for consistent output
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

void TradingLogger::log_trading_conditions(double daily_pnl, double exposure_pct, bool allowed) {
    log_message("     ┌─ TRADING CONDITIONS", "");
    
    std::ostringstream pnl_oss;
    pnl_oss << "     │ Daily P/L: " << format_percentage(daily_pnl) 
            << " (Limits: -100.000000% to 30.000000%)";
    log_message(pnl_oss.str(), "");
    
    std::ostringstream exp_oss;
    exp_oss << "     │ Exposure: " << std::fixed << std::setprecision(0) << exposure_pct 
            << "% (Max: 50%)";
    log_message(exp_oss.str(), "");
    
    if (allowed) {
        log_message("     │ PASS: All conditions met", "");
        log_message("     └─ Trading allowed", "");
    } else {
        log_message("     │ FAIL: Trading halted", "");
        log_message("     └─ Trading suspended", "");
    }
}

void TradingLogger::log_equity_update(double current_equity) {
    log_message("     ┌─ ", "");
    std::ostringstream oss;
    oss << "     │ Current Equity: " << format_currency(current_equity) << " (acct poll=5s)";
    log_message(oss.str(), "");
    log_message("     └─ ", "");
}

void TradingLogger::log_market_data_status(bool has_data, size_t data_points) {
    // Using unified log_message for consistent output
    
    if (has_data) {
        log_message("Market data available (" + std::to_string(data_points) + " points)", "");
    } else {
        log_message("No market data available", "");
    }
}

void TradingLogger::log_signal_analysis(const std::string& symbol, bool buy_signal, bool sell_signal) {
    // Using unified log_message for consistent output
    
    std::ostringstream oss;
    oss << symbol << " signals - BUY: " << (buy_signal ? "YES" : "NO") 
        << " | SELL: " << (sell_signal ? "YES" : "NO");
    
    log_message(oss.str(), "");
}

void TradingLogger::log_filter_results(bool atr_pass, bool volume_pass, bool doji_pass) {
    // Using unified log_message for consistent output
    
    std::ostringstream oss;
    oss << "Filters - ATR: " << (atr_pass ? "PASS" : "FAIL")
        << " | VOL: " << (volume_pass ? "PASS" : "FAIL")
        << " | DOJI: " << (doji_pass ? "PASS" : "FAIL");
    
    log_message(oss.str(), "");
}

void TradingLogger::log_position_sizing(double risk_amount, int quantity) {
    // Using unified log_message for consistent output
    
    std::ostringstream oss;
    oss << "Position sizing - Risk: " << format_currency(risk_amount) 
        << " | Qty: " << quantity;
    
    log_message(oss.str(), "");
}

void TradingLogger::log_order_intent(const std::string& side, double entry_price, double stop_loss, double take_profit) {
    // Using unified log_message for consistent output
    
    std::ostringstream oss;
    oss << side << " order intent - Entry: " << format_currency(entry_price)
        << " | SL: " << format_currency(stop_loss)
        << " | TP: " << format_currency(take_profit);
    
    log_message("ORDER", oss.str());
}

void TradingLogger::log_order_result(const std::string& order_id, bool success, const std::string& reason) {
    // Using unified log_message for consistent output
    
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
    // Using unified log_message for consistent output
    
    std::ostringstream oss;
    oss << "Position: " << current_quantity << " shares";
    if (unrealized_pnl != 0.0) {
        oss << " | Unrealized P/L: " << format_currency(unrealized_pnl);
    }
    
    log_message("POSITION", oss.str());
}

void TradingLogger::log_execution_time(const std::string& operation, long microseconds) {
    // Using unified log_message for consistent output
    
    std::ostringstream oss;
    oss << operation << " execution time: " << microseconds << "μs";
    
    log_message("PERF", oss.str());
}

void TradingLogger::log_system_health(const std::string& component, bool healthy, const std::string& details) {
    // Using unified log_message for consistent output
    
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

// Beautiful formatted detailed logs

void TradingLogger::log_loop_header(unsigned long loop_number) {
    log_message("", "");
    log_message("╔══════════════════════════════════════════════════════════════════════════════════╗", "");
    log_message("║                              TRADING LOOP #" + std::to_string(loop_number) + "                                     ║", "");
    log_message("╚══════════════════════════════════════════════════════════════════════════════════╝", "");
    log_message("", "");
}

void TradingLogger::log_header_and_config(const TraderConfig& config) {
    log_message("", "");
    log_message("╔══════════════════════════════════════════════════════════════════════════════════╗", "");
    log_message("║                                   ALPACA TRADER                                  ║", "");
    log_message("║                            Advanced Momentum Trading Bot                         ║", "");
    log_message("╚══════════════════════════════════════════════════════════════════════════════════╝", "");
    log_message("", "");
    log_message("     ┌─ CONFIGURATION:", "");
    log_message("     |  • Symbol: " + config.target.symbol, "");
    log_message("     |  • Risk per Trade: " + format_percentage(config.risk.risk_per_trade * 100), "");
    log_message("     |  • Risk/Reward Ratio: 1:" + std::to_string(config.strategy.rr_ratio), "");
    log_message("     |  • Loop Interval: " + std::to_string(config.timing.sleep_interval_sec) + " seconds", "");
    log_message("     └─ ", "");
    log_message("", "");
}

void TradingLogger::log_candle_and_signals(const ProcessedData& data, const StrategyLogic::SignalDecision& signals) {
    std::ostringstream oss;
    oss << "     │ Candle: O=$" << std::fixed << std::setprecision(0) << data.curr.o 
        << " H=$" << data.curr.h 
        << " L=$" << data.curr.l 
        << " C=$" << data.curr.c;
    log_message(oss.str(), "");
    
    std::ostringstream signals_oss;
    signals_oss << "     │ Signals: BUY=" << (signals.buy ? "YES" : "NO") 
                << " | SELL=" << (signals.sell ? "YES" : "NO");
    log_message(signals_oss.str(), "");
}

void TradingLogger::log_filters(const StrategyLogic::FilterResult& filters, const TraderConfig& config) {
    std::ostringstream oss;
    oss << "     │ Filters: ATR=" << (filters.atr_pass ? "PASS" : "FAIL") 
        << " (" << std::fixed << std::setprecision(2) << filters.atr_ratio << "x > " 
        << config.strategy.atr_multiplier_entry << "x) | VOL=" << (filters.vol_pass ? "PASS" : "FAIL")
        << " (" << std::fixed << std::setprecision(2) << filters.vol_ratio << "x > " 
        << config.strategy.volume_multiplier << "x) | DOJI=" << (filters.doji_pass ? "PASS" : "FAIL");
    log_message(oss.str(), "");
}

void TradingLogger::log_summary(const ProcessedData& data, const StrategyLogic::SignalDecision& signals, const StrategyLogic::FilterResult& filters) {
    std::ostringstream oss;
    oss << "     | Summary: C=" << std::fixed << std::setprecision(0) << data.curr.c 
        << " | SIG: B=" << (signals.buy ? "YES" : "NO") 
        << " S=" << (signals.sell ? "YES" : "NO") 
        << " | FIL: ATR" << (filters.atr_pass ? "+" : "-") 
        << " VOL" << (filters.vol_pass ? "+" : "-") 
        << " DOJI" << (filters.doji_pass ? "+" : "-") 
        << " | ATRx=" << std::fixed << std::setprecision(4) << filters.atr_ratio 
        << " VOLx=" << std::fixed << std::setprecision(4) << filters.vol_ratio 
        << " | EXP=" << std::fixed << std::setprecision(0) << data.exposure_pct << "%";
    log_message(oss.str(), "");
}

void TradingLogger::log_signal_analysis_start() {
    log_message("     ┌─ SIGNAL ANALYSIS (per-lap decisions)", "");
    log_message("     │", "");
}

void TradingLogger::log_signal_analysis_complete() {
    log_message("     │", "");
    log_message("     └─ Signal analysis complete", "");
}

void TradingLogger::log_filters_not_met_preview(double risk_amount, int quantity) {
    log_message("     │ ", "");
    log_message("     │ Filters Not Met", "");
    std::ostringstream oss;
    oss << "     │ Preview Sizing: Risk=" << format_currency(risk_amount) << " | Qty~" << quantity;
    log_message(oss.str(), "");
    log_message("     │-----------------------------------", "");
    log_message("     │ No trade executed", "");
    log_message("     └─----------------------------------- ", "");
}

void TradingLogger::log_position_size(double risk_amount, int quantity) {
    std::ostringstream oss;
    oss << "Position sizing - Risk: " << format_currency(risk_amount) << " | Qty: " << quantity;
    log_message(oss.str(), "");
}

void TradingLogger::log_current_position(int quantity) {
    std::ostringstream oss;
    oss << "Current position: " << quantity << " shares";
    log_message(oss.str(), "");
}

