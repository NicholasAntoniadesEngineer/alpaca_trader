// trader_logging.cpp
#include "core/trader_logging.hpp"
#include "utils/async_logger.hpp"
#include <sstream>

namespace TraderLogging {

void log_header_and_config(const TraderConfig& config) {
    log_message("", config.logging.log_file);
    log_message("╔══════════════════════════════════════════════════════════════════════════════════╗", config.logging.log_file);
    log_message("║                                   ALPACA TRADER                                  ║", config.logging.log_file);
    log_message("║                            Advanced Momentum Trading Bot                         ║", config.logging.log_file);
    log_message("╚══════════════════════════════════════════════════════════════════════════════════╝", config.logging.log_file);
    log_message("", config.logging.log_file);

    log_message("CONFIGURATION:", config.logging.log_file);
    {
        std::ostringstream oss; oss << "   • Symbol: " << config.target.symbol; log_message(oss.str(), config.logging.log_file);
    }
    {
        std::ostringstream oss; oss << "   • Risk per Trade: " << (config.risk.risk_per_trade * 100) << "%"; log_message(oss.str(), config.logging.log_file);
    }
    {
        std::ostringstream oss; oss << "   • Risk/Reward Ratio: 1:" << config.strategy.rr_ratio; log_message(oss.str(), config.logging.log_file);
    }
    {
        std::ostringstream oss; oss << "   • Loop Interval: " << config.timing.sleep_interval_sec << " seconds"; log_message(oss.str(), config.logging.log_file);
    }
    log_message("", config.logging.log_file);
}

void log_trader_started(const TraderConfig& config, double initial_equity) {
    log_message("Starting advanced momentum trader for " + config.target.symbol + ". Initial equity: " + std::to_string(initial_equity), config.logging.log_file);
}

void log_loop_header(unsigned long loop_number, const TraderConfig& cfg) {
    log_message("", cfg.logging.log_file);
    log_message("╔══════════════════════════════════════════════════════════════════════════════════╗", cfg.logging.log_file);
    log_message("║                              TRADING LOOP #" + std::to_string(loop_number) + "                                     ║", cfg.logging.log_file);
    log_message("╚══════════════════════════════════════════════════════════════════════════════════╝", cfg.logging.log_file);
}

void log_halted_header(const TraderConfig& cfg) {
    log_message("", cfg.logging.log_file);
    log_message("╔══════════════════════════════════════════════════════════════════════════════════╗", cfg.logging.log_file);
    log_message("║                              MARKET CLOSED / HALTED                              ║", cfg.logging.log_file);
    log_message("║    Conditions not met. Countdown to next check will display in the console.      ║", cfg.logging.log_file);
    log_message("╚══════════════════════════════════════════════════════════════════════════════════╝", cfg.logging.log_file);
}

void log_equity_status(double equity, const TraderConfig& cfg) {
    log_message("Current Equity: $" + std::to_string(static_cast<int>(equity)) +
                " (acct poll=" + std::to_string(cfg.timing.account_poll_sec) + "s)", cfg.logging.log_file);
}

void log_trading_conditions_start(const TraderConfig& cfg) { log_message("   ┌─ TRADING CONDITIONS", cfg.logging.log_file); }
void log_market_closed(const TraderConfig& cfg) { log_message("   │ FAIL: Market closed or outside trading hours", cfg.logging.log_file); }
void log_daily_pnl_line(double daily_pnl, const TraderConfig& cfg) {
    log_message("   │ Daily P/L: " + std::to_string(daily_pnl * 100).substr(0,5) + "% "
                + "(Limits: " + std::to_string(cfg.risk.daily_max_loss * 100) + "% to "
                + std::to_string(cfg.risk.daily_profit_target * 100) + "%)", cfg.logging.log_file);
}
void log_pnl_limit_reached(const TraderConfig& cfg) { log_message("   │ FAIL: Daily P/L limit reached", cfg.logging.log_file); }
void log_exposure_line(double exposure_pct, const TraderConfig& cfg) {
    log_message("   │ Exposure: " + std::to_string(static_cast<int>(exposure_pct)) + "% (Max: " +
                std::to_string(static_cast<int>(cfg.risk.max_exposure_pct)) + "%)", cfg.logging.log_file);
}
void log_exposure_limit_reached(const TraderConfig& cfg) { log_message("   │ FAIL: Exposure limit exceeded", cfg.logging.log_file); }
void log_trading_allowed(const TraderConfig& cfg) { log_message("   │ PASS: All conditions met", cfg.logging.log_file); log_message("   └─ Trading allowed", cfg.logging.log_file); }
void log_trading_halted_tail(const TraderConfig& cfg) { log_message("   └─ Trading halted", cfg.logging.log_file); }

void log_market_data_header(const TraderConfig& cfg) { log_message("   ┌─ MARKET DATA", cfg.logging.log_file); }
void log_no_market_data(const TraderConfig& cfg) { log_message("   │ FAIL: No market data available", cfg.logging.log_file); }
void log_insufficient_data(size_t got, int needed, const TraderConfig& cfg) {
    log_message("   │ FAIL: Insufficient data: got " + std::to_string(got) + ", need " + std::to_string(needed), cfg.logging.log_file);
}
void log_indicator_failure(const TraderConfig& cfg) { log_message("   │ FAIL: ATR calculation failed", cfg.logging.log_file); log_message("   └─ Technical analysis failed", cfg.logging.log_file); }
void log_position_market_summary(const ProcessedData& data, const TraderConfig& cfg) {
    log_message("   │", cfg.logging.log_file);
    log_message("   │ MARKET: " + cfg.target.symbol + " @ $" + std::to_string(static_cast<int>(data.curr.c)) +
                " | ATR: " + std::to_string(data.atr).substr(0, cfg.ux.log_float_chars) +
                " | Vol: " + std::to_string(data.curr.v/1000) + "K", cfg.logging.log_file);
    log_message("   │ POSITION: " + std::to_string(data.pos_details.qty) + " shares | " +
                "Value: $" + std::to_string(static_cast<int>(data.pos_details.current_value)) +
                " | P/L: $" + std::to_string(static_cast<int>(data.pos_details.unrealized_pl)) +
                " | Exposure: " + std::to_string(static_cast<int>(data.exposure_pct)) + "%", cfg.logging.log_file);
    log_message("   └─ Data collection complete", cfg.logging.log_file);
}
void log_computing_indicators_start(const TraderConfig& cfg) { log_message("   │ PASS: Computing technical indicators...", cfg.logging.log_file); }
void log_getting_position_and_account(const TraderConfig& cfg) { log_message("   │ PASS: Getting position and account details...", cfg.logging.log_file); }
void log_market_data_collection_failed(const TraderConfig& cfg) { log_message("   └─ Market data collection failed", cfg.logging.log_file); }
void log_missing_bracket_warning(const TraderConfig& cfg) { log_message("   WARNING: Position open but no bracket orders detected!", cfg.logging.log_file); }

void log_signal_analysis_start(const TraderConfig& cfg) { log_message("   ┌─ SIGNAL ANALYSIS (per-lap decisions)", cfg.logging.log_file); }
void log_candle_and_signals(const ProcessedData& data, const StrategyLogic::SignalDecision& sd, const TraderConfig& cfg) {
    log_message("   │ Candle: O=$" + std::to_string(static_cast<int>(data.curr.o)) +
                " H=$" + std::to_string(static_cast<int>(data.curr.h)) +
                " L=$" + std::to_string(static_cast<int>(data.curr.l)) +
                " C=$" + std::to_string(static_cast<int>(data.curr.c)), cfg.logging.log_file);
    log_message("   │ Signals: BUY=" + std::string(sd.buy ? "YES" : "NO") +
                " | SELL=" + std::string(sd.sell ? "YES" : "NO"), cfg.logging.log_file);
}
void log_filters(const StrategyLogic::FilterResult& fr, const TraderConfig& cfg) {
    log_message("   │ Filters: ATR=" + std::string(fr.atr_pass ? "PASS" : "FAIL") +
                " (" + std::to_string(fr.atr_ratio).substr(0,4) + "x > " + std::to_string(cfg.strategy.atr_multiplier_entry) + "x)"
                " | VOL=" + std::string(fr.vol_pass ? "PASS" : "FAIL") +
                " (" + std::to_string(fr.vol_ratio).substr(0,4) + "x > " + std::to_string(cfg.strategy.volume_multiplier) + "x)"
                " | DOJI=" + std::string(fr.doji_pass ? "PASS" : "FAIL"), cfg.logging.log_file);
}
void log_summary(const ProcessedData& data, const StrategyLogic::SignalDecision& sd, const StrategyLogic::FilterResult& fr, const TraderConfig& cfg) {
    std::string buy_s = sd.buy ? "YES" : "NO";
    std::string sell_s = sd.sell ? "YES" : "NO";
    std::string atr_s = fr.atr_pass ? "+" : "-";
    std::string vol_s = fr.vol_pass ? "+" : "-";
    std::string doji_s = fr.doji_pass ? "+" : "-";
    log_message("   │ SUMMARY: C=" + std::to_string(static_cast<int>(data.curr.c)) +
                " | SIG: B=" + buy_s + " S=" + sell_s +
                " | FIL: ATR" + atr_s + " VOL" + vol_s + " DOJI" + doji_s +
                " | ATRx=" + std::to_string(fr.atr_ratio).substr(0, cfg.ux.log_float_chars) +
                " VOLx=" + std::to_string(fr.vol_ratio).substr(0, cfg.ux.log_float_chars) +
                " | EXP=" + std::to_string(static_cast<int>(data.exposure_pct)) + "%", cfg.logging.log_file);
}
void log_filters_not_met_preview(double risk_prev, int qty_prev, const TraderConfig& cfg) {
    log_message("   │ RESULT: Signal filters not met", cfg.logging.log_file);
    log_message("   │ Preview Sizing: Risk=$" + std::to_string(risk_prev).substr(0, cfg.ux.log_float_chars) +
                " | Qty~" + std::to_string(qty_prev), cfg.logging.log_file);
    log_message("   └─ No trade executed", cfg.logging.log_file);
}
void log_filters_pass(const TraderConfig& cfg) { log_message("   │ PASS: All filters passed", cfg.logging.log_file); }
void log_current_position(int current_qty, const TraderConfig& cfg) { log_message("   │ Current position: " + std::to_string(current_qty) + " shares", cfg.logging.log_file); }
void log_position_size(double risk_amount, int qty, const TraderConfig& cfg) { log_message("   │ Position Size: Risk=$" + std::to_string(risk_amount).substr(0,4) + " | Qty=" + std::to_string(qty) + " shares", cfg.logging.log_file); }
void log_qty_too_small(const TraderConfig& cfg) { log_message("   │ FAIL: Calculated quantity too small", cfg.logging.log_file); log_message("   └─ No trade executed", cfg.logging.log_file); }
void log_buy_triggered(const TraderConfig& cfg) { log_message("   │ BUY SIGNAL TRIGGERED", cfg.logging.log_file); }
void log_sell_triggered(const TraderConfig& cfg) { log_message("   │ SELL SIGNAL TRIGGERED", cfg.logging.log_file); }
void log_close_position_first(const char* side, const TraderConfig& cfg) { log_message(std::string("   │ Closing ") + side + " position first...", cfg.logging.log_file); }
void log_open_position_details(const char* label, double entry, double sl, double tp, const TraderConfig& cfg) {
    log_message(std::string("   │ ") + label, cfg.logging.log_file);
    log_message("   │ Entry: $" + std::to_string(static_cast<int>(entry)) +
                " | SL: $" + std::to_string(static_cast<int>(sl)) +
                " | TP: $" + std::to_string(static_cast<int>(tp)), cfg.logging.log_file);
}
void log_position_limits_reached(const char* side, const TraderConfig& cfg) { log_message(std::string("   │ ") + side + " signal but position limits reached", cfg.logging.log_file); }
void log_no_valid_pattern(const TraderConfig& cfg) { log_message("   │ No valid breakout pattern", cfg.logging.log_file); }
void log_signal_analysis_complete(const TraderConfig& cfg) { log_message("   └─ Signal analysis complete", cfg.logging.log_file); }

} // namespace TraderLogging


