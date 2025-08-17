// Trader.cpp
#include "core/trader.hpp"
#include "utils/indicators.hpp"
#include "utils/async_logger.hpp"
#include <thread>
#include <cmath>
#include <chrono>
#include <sstream>

Trader::Trader(const TraderConfig& cfg, AlpacaClient& clientRef, AccountManager& accountMgr)
    : config_ptr(&cfg), client(clientRef), account_manager(accountMgr), initial_equity(0.0) {
    initial_equity = initialize_trader();
}

Trader::~Trader() {}

double Trader::initialize_trader() {
    const TraderConfig& config = *config_ptr;
    double eq = account_manager.get_equity();
    log_message("Starting advanced momentum trader for " + config.target.symbol + ". Initial equity: " + std::to_string(eq), config.logging.log_file);
    return eq;
}

bool Trader::can_trade(double exposure_pct) {
    const TraderConfig& config = *config_ptr;
    log_message("   ┌─ TRADING CONDITIONS", config.logging.log_file);

    if (!client.is_core_trading_hours()) {
        log_message("   │ FAIL: Market closed or outside trading hours", config.logging.log_file);
        log_message("   └─ Trading halted", config.logging.log_file);
        return false;
    }


    double equity = account_manager.get_equity();
    double daily_pnl = (initial_equity == 0.0) ? 0.0 : (equity - initial_equity) / initial_equity;
    log_message("   │ Daily P/L: " + std::to_string(daily_pnl * 100).substr(0,5) + "% "
                + "(Limits: " + std::to_string(config.risk.daily_max_loss * 100) + "% to "
                + std::to_string(config.risk.daily_profit_target * 100) + "%)", config.logging.log_file);

    if (daily_pnl <= config.risk.daily_max_loss || daily_pnl >= config.risk.daily_profit_target) {
        log_message("   │ FAIL: Daily P/L limit reached", config.logging.log_file);
        log_message("   └─ Trading halted", config.logging.log_file);
        return false;
    }

    log_message("   │ Exposure: " + std::to_string(static_cast<int>(exposure_pct)) + "% (Max: " +
                std::to_string(static_cast<int>(config.risk.max_exposure_pct)) + "%)", config.logging.log_file);
    if (exposure_pct > config.risk.max_exposure_pct) {
        log_message("   │ FAIL: Exposure limit exceeded", config.logging.log_file);
        log_message("   └─ Trading halted", config.logging.log_file);
        return false;
    }

    log_message("   │ PASS: All conditions met", config.logging.log_file);
    log_message("   └─ Trading allowed", config.logging.log_file);
    return true;
}

ProcessedData Trader::fetch_and_process_data() {
    const TraderConfig& config = *config_ptr;
    ProcessedData data;
    log_message("   ┌─ MARKET DATA", config.logging.log_file);

    BarRequest br{config.target.symbol, config.strategy.atr_period + config.timing.bar_buffer};
    auto bars = client.get_recent_bars(br);
    if (bars.size() < static_cast<size_t>(config.strategy.atr_period + 2)) {
        if (bars.size() == 0) {
            log_message("   │ FAIL: No market data available", config.logging.log_file);
        } else {
            log_message("   │ FAIL: Insufficient data: got " + std::to_string(bars.size()) + ", need " + std::to_string(config.strategy.atr_period + 2), config.logging.log_file);
        }
        log_message("   └─ Market data collection failed", config.logging.log_file);
        return data;
    }

    log_message("   │ PASS: Computing technical indicators...", config.logging.log_file);

    std::vector<double> highs, lows, closes;
    std::vector<long long> volumes;
    for (const auto& b : bars) {
        highs.push_back(b.h);
        lows.push_back(b.l);
        closes.push_back(b.c);
        volumes.push_back(b.v);
    }

    data.atr = calculate_atr(highs, lows, closes, config.strategy.atr_period);
        if (data.atr == 0.0) {
        log_message("   │ FAIL: ATR calculation failed", config.logging.log_file);
        log_message("   └─ Technical analysis failed", config.logging.log_file);
        return data;
    }
    data.avg_atr = calculate_atr(highs, lows, closes, config.strategy.atr_period * config.strategy.avg_atr_multiplier);
    data.avg_vol = calculate_avg_volume(volumes, config.strategy.atr_period);
    data.curr = bars.back();
    data.prev = bars[bars.size() - 2];

    log_message("   │ PASS: Getting position and account details...", config.logging.log_file);
    SymbolRequest sr{config.target.symbol};
    data.pos_details = account_manager.get_position_details(sr);
    data.open_orders = account_manager.get_open_orders_count(sr);
    double equity = account_manager.get_equity();
    data.exposure_pct = (equity > 0.0) ? (std::abs(data.pos_details.current_value) / equity) * 100.0 : 0.0;

    log_message("   │", config.logging.log_file);
    log_message("   │ MARKET: " + config.target.symbol + " @ $" + std::to_string(static_cast<int>(data.curr.c)) +
                " | ATR: " + std::to_string(data.atr).substr(0, config.ux.log_float_chars) +
                " | Vol: " + std::to_string(data.curr.v/1000) + "K", config.logging.log_file);
    log_message("   │ POSITION: " + std::to_string(data.pos_details.qty) + " shares | " +
                "Value: $" + std::to_string(static_cast<int>(data.pos_details.current_value)) +
                " | P/L: $" + std::to_string(static_cast<int>(data.pos_details.unrealized_pl)) +
                " | Exposure: " + std::to_string(static_cast<int>(data.exposure_pct)) + "%", config.logging.log_file);
    log_message("   └─ Data collection complete", config.logging.log_file);

    if (data.pos_details.qty != 0 && data.open_orders == 0) {
        log_message("   WARNING: Position open but no bracket orders detected!", config.logging.log_file);
    }

    return data;
}

void Trader::evaluate_and_execute_signal(const ProcessedData& data, double equity) {
    const TraderConfig& config = *config_ptr;
    log_message("   ┌─ SIGNAL ANALYSIS (per-lap decisions)", config.logging.log_file);

    int current_qty = data.pos_details.qty;
    bool is_long = current_qty > 0;
    bool is_short = current_qty < 0;
    const bool buy_signal = (data.curr.c > data.curr.o) &&
                            (data.curr.h > data.prev.h) &&
                            (data.curr.l >= data.prev.l);
    const bool sell_signal = (data.curr.c < data.curr.o) &&
                             (data.curr.l < data.prev.l) &&
                             (data.curr.h <= data.prev.h);

    log_message("   │ Candle: O=$" + std::to_string(static_cast<int>(data.curr.o)) +
                " H=$" + std::to_string(static_cast<int>(data.curr.h)) +
                " L=$" + std::to_string(static_cast<int>(data.curr.l)) +
                " C=$" + std::to_string(static_cast<int>(data.curr.c)), config.logging.log_file);
    log_message("   │ Signals: BUY=" + std::string(buy_signal ? "YES" : "NO") +
                " | SELL=" + std::string(sell_signal ? "YES" : "NO"), config.logging.log_file);

    bool atr_filter = data.atr > config.strategy.atr_multiplier_entry * data.avg_atr;
    bool vol_filter = data.curr.v > config.strategy.volume_multiplier * data.avg_vol;
    bool doji_filter = !is_doji(data.curr.o, data.curr.h, data.curr.l, data.curr.c);
    bool filters_pass = atr_filter && vol_filter && doji_filter;

    // Ratios for readability
    double atr_ratio = (data.avg_atr > 0.0) ? (data.atr / data.avg_atr) : 0.0;
    double vol_ratio = (data.avg_vol > 0.0) ? (static_cast<double>(data.curr.v) / data.avg_vol) : 0.0;
    log_message("   │ Filters: ATR=" + std::string(atr_filter ? "PASS" : "FAIL") +
                " (" + std::to_string(atr_ratio).substr(0,4) + "x > " + std::to_string(config.strategy.atr_multiplier_entry) + "x)"
                " | VOL=" + std::string(vol_filter ? "PASS" : "FAIL") +
                " (" + std::to_string(vol_ratio).substr(0,4) + "x > " + std::to_string(config.strategy.volume_multiplier) + "x)"
                " | DOJI=" + std::string(doji_filter ? "PASS" : "FAIL"), config.logging.log_file);

    // Compact summary line per loop
    std::string buy_s = buy_signal ? "YES" : "NO";
    std::string sell_s = sell_signal ? "YES" : "NO";
    std::string atr_s = atr_filter ? "+" : "-";
    std::string vol_s = vol_filter ? "+" : "-";
    std::string doji_s = doji_filter ? "+" : "-";
    log_message("   │ SUMMARY: C=" + std::to_string(static_cast<int>(data.curr.c)) +
                " | SIG: B=" + buy_s + " S=" + sell_s +
                " | FIL: ATR" + atr_s + " VOL" + vol_s + " DOJI" + doji_s +
                " | ATRx=" + std::to_string(atr_ratio).substr(0, config.ux.log_float_chars) + " VOLx=" + std::to_string(vol_ratio).substr(0, config.ux.log_float_chars) +
                " | EXP=" + std::to_string(static_cast<int>(data.exposure_pct)) + "%", config.logging.log_file);

    if (!filters_pass) {
        // Sizing preview even when not trading for insight
        double risk_prev = data.atr > 0.0 ? data.atr : 1.0;
        int qty_prev = static_cast<int>(std::floor((equity * config.risk.risk_per_trade) / risk_prev));
        log_message("   │ RESULT: Signal filters not met", config.logging.log_file);
        log_message("   │ Preview Sizing: Risk=$" + std::to_string(risk_prev).substr(0, config.ux.log_float_chars) +
                    " | Qty~" + std::to_string(qty_prev), config.logging.log_file);
        log_message("   └─ No trade executed", config.logging.log_file);
        return;
    }

    log_message("   │ PASS: All filters passed", config.logging.log_file);

    if (current_qty != 0) {
        log_message("   │ Current position: " + std::to_string(current_qty) + " shares", config.logging.log_file);
    }

    double risk_amount = data.atr;
    double size_multiplier = (current_qty != 0 && config.risk.allow_multiple_positions) ? config.risk.scale_in_multiplier : 1.0;
    int qty = static_cast<int>(std::floor((equity * config.risk.risk_per_trade * size_multiplier) / risk_amount));

    log_message("   │ Position Size: Risk=$" + std::to_string(risk_amount).substr(0,4) +
                " | Qty=" + std::to_string(qty) + " shares", config.logging.log_file);

    if (qty < 1) {
        log_message("   │ FAIL: Calculated quantity too small", config.logging.log_file);
        log_message("   └─ No trade executed", config.logging.log_file);
        return;
    }

    if (buy_signal) {
        log_message("   │ BUY SIGNAL TRIGGERED", config.logging.log_file);
        if (is_short && config.risk.close_on_reverse) {
            log_message("   │ Closing short position first...", config.logging.log_file);
            client.close_position(ClosePositionRequest{current_qty});
        }
        if (is_long && config.risk.allow_multiple_positions) {
            double sl = data.curr.c - risk_amount;
            double tp = data.curr.c + (config.strategy.rr_ratio * risk_amount);
            log_message("   │ Scaling into long position", config.logging.log_file);
            log_message("   │ Entry: $" + std::to_string(static_cast<int>(data.curr.c)) +
                        " | SL: $" + std::to_string(static_cast<int>(sl)) +
                        " | TP: $" + std::to_string(static_cast<int>(tp)), config.logging.log_file);
            client.place_bracket_order(OrderRequest{"buy", qty, tp, sl});
        } else if (!is_long && !is_short) {
            double sl = data.curr.c - risk_amount;
            double tp = data.curr.c + (config.strategy.rr_ratio * risk_amount);
            log_message("   │ Opening new long position", config.logging.log_file);
            log_message("   │ Entry: $" + std::to_string(static_cast<int>(data.curr.c)) +
                        " | SL: $" + std::to_string(static_cast<int>(sl)) +
                        " | TP: $" + std::to_string(static_cast<int>(tp)), config.logging.log_file);
            client.place_bracket_order(OrderRequest{"buy", qty, tp, sl});
        } else {
            log_message("   │ Buy signal but position limits reached", config.logging.log_file);
        }
    } else if (sell_signal) {
        log_message("   │ SELL SIGNAL TRIGGERED", config.logging.log_file);
        if (is_long && config.risk.close_on_reverse) {
            log_message("   │ Closing long position first...", config.logging.log_file);
            client.close_position(ClosePositionRequest{current_qty});
        }
        if (is_short && config.risk.allow_multiple_positions) {
            double sl = data.curr.c + risk_amount;
            double tp = data.curr.c - (config.strategy.rr_ratio * risk_amount);
            log_message("   │ Scaling into short position", config.logging.log_file);
            log_message("   │ Entry: $" + std::to_string(static_cast<int>(data.curr.c)) +
                        " | SL: $" + std::to_string(static_cast<int>(sl)) +
                        " | TP: $" + std::to_string(static_cast<int>(tp)), config.logging.log_file);
            client.place_bracket_order(OrderRequest{"sell", qty, tp, sl});
        } else if (!is_long && !is_short) {
            double sl = data.curr.c + risk_amount;
            double tp = data.curr.c - (config.strategy.rr_ratio * risk_amount);
            log_message("   │ Opening new short position", config.logging.log_file);
            log_message("   │ Entry: $" + std::to_string(static_cast<int>(data.curr.c)) +
                        " | SL: $" + std::to_string(static_cast<int>(tp)) +
                        " | TP: $" + std::to_string(static_cast<int>(sl)), config.logging.log_file);
            client.place_bracket_order(OrderRequest{"sell", qty, tp, sl});
        } else {
            log_message("   │ Sell signal but position limits reached", config.logging.log_file);
        }
    } else {
        log_message("   │ No valid breakout pattern", config.logging.log_file);
    }

    log_message("   └─ Signal analysis complete", config.logging.log_file);
}

void Trader::decision_loop() {
    while (running_ptr && running_ptr->load()) {
        // Wait for fresh data
        wait_for_fresh_data();
        if (!running_ptr->load()) break;
        
        // Get current market and account snapshots
        auto snapshots = get_current_snapshots();
        MarketSnapshot market = snapshots.first;
        AccountSnapshot account = snapshots.second;
        
        // Display loop header
        display_loop_header();
        
        // Check if we can trade
        if (!can_trade(account.exposure_pct)) {
            handle_trading_halt();
            continue;
        }
        
        // Display current equity status
        display_equity_status(account.equity);
        
        // Process data and execute signals
        process_trading_cycle(market, account);
        
        // Countdown to next cycle
        countdown_to_next_cycle();
    }
}

void Trader::wait_for_fresh_data() {
    std::unique_lock<std::mutex> lock(*state_mtx_ptr);
    data_cv_ptr->wait_for(lock, std::chrono::seconds(1), [&]{
        return (has_fresh_market_ptr && has_fresh_market_ptr->load()) && (has_fresh_account_ptr && has_fresh_account_ptr->load());
    });
    if (!running_ptr->load()) return;
    if (!has_fresh_market_ptr->load()) return;
    
    has_fresh_market_ptr->store(false);
    lock.unlock();
}

std::pair<MarketSnapshot, AccountSnapshot> Trader::get_current_snapshots() {
    MarketSnapshot market = *market_snapshot_ptr;
    AccountSnapshot account = *account_snapshot_ptr;
    return {market, account};
}

void Trader::display_loop_header() {
    const TraderConfig& config = *config_ptr;
    loop_counter.fetch_add(1);
    std::string loop_line = "╔══════════════════════════════════════════════════════════════════════════════════╗";
    log_message("", config.logging.log_file);
    log_message(loop_line, config.logging.log_file);
    log_message("║                              TRADING LOOP #" + std::to_string(loop_counter.load()) + "                                     ║", config.logging.log_file);
    log_message("╚══════════════════════════════════════════════════════════════════════════════════╝", config.logging.log_file);
}

void Trader::handle_trading_halt() {
    const TraderConfig& config = *config_ptr;
    log_message("", config.logging.log_file);
    log_message("╔══════════════════════════════════════════════════════════════════════════════════╗", config.logging.log_file);
    log_message("║                              MARKET CLOSED / HALTED                              ║", config.logging.log_file);
    log_message("║    Conditions not met. Countdown to next check will display in the console.     ║", config.logging.log_file);
    log_message("╚══════════════════════════════════════════════════════════════════════════════════╝", config.logging.log_file);
    
    // Countdown while halted
    int halt_secs = config.timing.halt_sleep_min * 60;
    for (int s = halt_secs; s > 0 && running_ptr->load(); --s) {
        log_inline_status("   ⏳ Halted: next check in " + std::to_string(s) + "s   ");
        std::this_thread::sleep_for(std::chrono::seconds(config.timing.countdown_tick_sec));
    }
    end_inline_status();
}

void Trader::display_equity_status(double equity) {
    const TraderConfig& config = *config_ptr;
    log_message("Current Equity: $" + std::to_string(static_cast<int>(equity)) +
                " (acct poll=" + std::to_string(config.timing.account_poll_sec) + "s)", config.logging.log_file);
}

void Trader::process_trading_cycle(const MarketSnapshot& market, const AccountSnapshot& account) {
    // Build ProcessedData from snapshots for decision function
    ProcessedData pd;
    pd.atr = market.atr;
    pd.avg_atr = market.avg_atr;
    pd.avg_vol = market.avg_vol;
    pd.curr = market.curr;
    pd.prev = market.prev;
    pd.pos_details = account.pos_details;
    pd.open_orders = account.open_orders;
    pd.exposure_pct = account.exposure_pct;

    evaluate_and_execute_signal(pd, account.equity);
}

void Trader::countdown_to_next_cycle() {
    const TraderConfig& config = *config_ptr;
    // Per-loop countdown to next cycle (visual heartbeat)
    int sleep_secs = config.timing.sleep_interval_sec;
    for (int s = sleep_secs; s > 0 && running_ptr->load(); --s) {
        log_inline_status("   ⏳ Next loop in " + std::to_string(s) + "s   ");
        std::this_thread::sleep_for(std::chrono::seconds(config.timing.countdown_tick_sec));
    }
    end_inline_status();
}

void Trader::run() {
    // Header (unchanged)
    const TraderConfig& config = *config_ptr;
    log_message("", config.logging.log_file);
    log_message("╔══════════════════════════════════════════════════════════════════════════════════╗", config.logging.log_file);
    log_message("║                                   ALPACA TRADER                                 ║", config.logging.log_file);
    log_message("║                            Advanced Momentum Trading Bot                         ║", config.logging.log_file);
    log_message("╚══════════════════════════════════════════════════════════════════════════════════╝", config.logging.log_file);
    log_message("", config.logging.log_file);
    
    log_message("CONFIGURATION:", config.logging.log_file);
    {
        std::ostringstream oss;
        oss << "   • Symbol: " << config.target.symbol;
        log_message(oss.str(), config.logging.log_file);
    }
    {
        std::ostringstream oss;
        oss << "   • Risk per Trade: " << (config.risk.risk_per_trade * 100) << "%";
        log_message(oss.str(), config.logging.log_file);
    }
    {
        std::ostringstream oss;
        oss << "   • Risk/Reward Ratio: 1:" << config.strategy.rr_ratio;
        log_message(oss.str(), config.logging.log_file);
    }
    {
        std::ostringstream oss;
        oss << "   • Loop Interval: " << config.timing.sleep_interval_sec << " seconds";
        log_message(oss.str(), config.logging.log_file);
    }
    log_message("", config.logging.log_file);
    
    // In new design, main() owns market/account threads and running flag.
}

void Trader::attach_shared_state(std::mutex& mtx,
                                 std::condition_variable& cv,
                                 MarketSnapshot& market,
                                 AccountSnapshot& account,
                                 std::atomic<bool>& has_market,
                                 std::atomic<bool>& has_account,
                                 std::atomic<bool>& running_flag) {
    state_mtx_ptr = &mtx;
    data_cv_ptr = &cv;
    market_snapshot_ptr = &market;
    account_snapshot_ptr = &account;
    has_fresh_market_ptr = &has_market;
    has_fresh_account_ptr = &has_account;
    running_ptr = &running_flag;
}

void Trader::start_decision_thread() {
    // Start only decision thread
    decision_thread = std::thread([this]{ set_log_thread_tag("DECIDE"); decision_loop(); });
}

void Trader::join_decision_thread() {
    if (decision_thread.joinable()) decision_thread.join();
}
