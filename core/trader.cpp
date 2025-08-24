// Trader.cpp
#include "trader.hpp"
#include "risk_logic.hpp"
#include "market_processing.hpp"
#include "../utils/async_logger.hpp"
#include "../threads/platform/thread_control.hpp"
#include <thread>
#include <cmath>
#include <chrono>
#include <cassert>

namespace {
    enum class OrderSide { Buy, Sell };
    inline const char* to_side_string(OrderSide s) { return s == OrderSide::Buy ? "buy" : "sell"; }
}

Trader::Trader(const TraderConfig& cfg, AlpacaClient& clientRef, AccountManager& accountMgr)
    : services{cfg, clientRef, accountMgr} {
    runtime.initial_equity = initialize_trader();
}

Trader::~Trader() {}

double Trader::initialize_trader() {
    double eq = services.account_manager.get_equity();
    TraderLogging::log_trader_started(services.config, eq);
    return eq;
}

bool Trader::can_trade(double exposure_pct) {
    TraderLogging::log_trading_conditions_start(services.config);

    RiskLogic::TradeGateInput in;
    in.initial_equity = runtime.initial_equity;
    in.current_equity = services.account_manager.get_equity();
    in.exposure_pct = exposure_pct;
    // in.core_trading_hours = services.client.is_core_trading_hours();
    // this is failing which has been effecting ability to reade
    in.core_trading_hours = true;

    RiskLogic::TradeGateResult res = RiskLogic::evaluate_trade_gate(in, services.config);

    if (!res.hours_ok) {
        TraderLogging::log_market_closed(services.config);
        TraderLogging::log_trading_halted_tail(services.config);
        return false;
    }

    TraderLogging::log_daily_pnl_line(res.daily_pnl, services.config);
    if (!res.pnl_ok) {
        TraderLogging::log_pnl_limit_reached(services.config);
        TraderLogging::log_trading_halted_tail(services.config);
        return false;
    }

    TraderLogging::log_exposure_line(exposure_pct, services.config);
    if (!res.exposure_ok) {
        TraderLogging::log_exposure_limit_reached(services.config);
        TraderLogging::log_trading_halted_tail(services.config);
        return false;
    }

    TraderLogging::log_trading_allowed(services.config);
    return true;
}

ProcessedData Trader::fetch_and_process_data() {
    ProcessedData data;
    TraderLogging::log_market_data_header(services.config);

    BarRequest br{services.config.target.symbol, services.config.strategy.atr_period + services.config.timing.bar_buffer};
    auto bars = services.client.get_recent_bars(br);
    if (bars.size() < static_cast<size_t>(services.config.strategy.atr_period + 2)) {
        if (bars.size() == 0) {
            TraderLogging::log_no_market_data(services.config);
        } else {
            TraderLogging::log_insufficient_data(bars.size(), services.config.strategy.atr_period + 2, services.config);
        }
        TraderLogging::log_market_data_collection_failed(services.config);
        return data;
    }

    TraderLogging::log_computing_indicators_start(services.config);

    data = MarketProcessing::compute_processed_data(bars, services.config);
    if (data.atr == 0.0) {
        TraderLogging::log_indicator_failure(services.config);
        return data;
    }

    TraderLogging::log_getting_position_and_account(services.config);
    SymbolRequest sr{services.config.target.symbol};
    data.pos_details = services.account_manager.get_position_details(sr);
    data.open_orders = services.account_manager.get_open_orders_count(sr);
    double equity = services.account_manager.get_equity();
    data.exposure_pct = (equity > 0.0) ? (std::abs(data.pos_details.current_value) / equity) * 100.0 : 0.0;

    TraderLogging::log_position_market_summary(data, services.config);

    if (data.pos_details.qty != 0 && data.open_orders == 0) {
        TraderLogging::log_missing_bracket_warning(services.config);
    }

    return data;
}

void Trader::evaluate_and_execute_signal(const ProcessedData& data, double equity) 
{
    TraderLogging::log_signal_analysis_start(services.config);
    int current_qty = data.pos_details.qty;

    // Step 1: Detect signals and log candle/signal info
    StrategyLogic::SignalDecision signal_decision = detect_signals(data);
    TraderLogging::log_candle_and_signals(data, signal_decision, services.config);

    // Step 2: Evaluate filters and log details
    StrategyLogic::FilterResult filter_result = evaluate_filters(data);
    TraderLogging::log_filters(filter_result, services.config);
    TraderLogging::log_summary(data, signal_decision, filter_result, services.config);

    // Step 3: Early return if filters fail (with sizing preview)
    if (!filter_result.all_pass) {
        double risk_prev = data.atr > 0.0 ? data.atr : 1.0;
        int qty_prev = static_cast<int>(std::floor((equity * services.config.risk.risk_per_trade) / risk_prev));
        TraderLogging::log_signal_analysis_complete(services.config);
        TraderLogging::log_filters_not_met_preview(risk_prev, qty_prev, services.config);
        return;
    }
    TraderLogging::log_filters_pass(services.config);

    // Step 4: Calculate position sizing and validate
    TraderLogging::log_current_position(current_qty, services.config);
    StrategyLogic::PositionSizing sizing = calculate_position_sizing(data, equity, current_qty);
    TraderLogging::log_position_size(sizing.risk_amount, sizing.quantity, services.config);
    if (sizing.quantity < 1) {
        TraderLogging::log_qty_too_small(services.config);
        TraderLogging::log_signal_analysis_complete(services.config);
        return;
    }

    // Step 5: Execute the trade decision
    execute_trade(data, current_qty, sizing, signal_decision);

    // Final completion log
    TraderLogging::log_signal_analysis_complete(services.config);
}

StrategyLogic::SignalDecision Trader::detect_signals(const ProcessedData& data) const {
    return StrategyLogic::detect_signals(data);
}

StrategyLogic::FilterResult Trader::evaluate_filters(const ProcessedData& data) const {
    return StrategyLogic::evaluate_filters(data, services.config);
}

StrategyLogic::PositionSizing Trader::calculate_position_sizing(const ProcessedData& data, double equity, int current_qty) const {
    return StrategyLogic::calculate_position_sizing(data, equity, current_qty, services.config);
}

void Trader::execute_trade(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& sd) {
    bool is_long = current_qty > 0;
    bool is_short = current_qty < 0;

    if (sd.buy) {
        TraderLogging::log_buy_triggered(services.config);
        if (is_short && services.config.risk.close_on_reverse) {
            TraderLogging::log_close_position_first("long", services.config);
            services.client.close_position(ClosePositionRequest{current_qty});
        }
        StrategyLogic::ExitTargets targets = StrategyLogic::compute_exit_targets(to_side_string(OrderSide::Buy), data.curr.c, sizing.risk_amount, services.config.strategy.rr_ratio);
        if (is_long && services.config.risk.allow_multiple_positions) {
            TraderLogging::log_open_position_details("Scaling into long position", data.curr.c, targets.stop_loss, targets.take_profit, services.config);
            services.client.place_bracket_order(OrderRequest{to_side_string(OrderSide::Buy), sizing.quantity, targets.take_profit, targets.stop_loss});
        } else if (!is_long && !is_short) {
            TraderLogging::log_open_position_details("Opening new long position", data.curr.c, targets.stop_loss, targets.take_profit, services.config);
            services.client.place_bracket_order(OrderRequest{to_side_string(OrderSide::Buy), sizing.quantity, targets.take_profit, targets.stop_loss});
        } else {
            TraderLogging::log_position_limits_reached("Buy", services.config);
        }
    } else if (sd.sell) {
        TraderLogging::log_sell_triggered(services.config);
        if (is_long && services.config.risk.close_on_reverse) {
            TraderLogging::log_close_position_first("short", services.config);
            services.client.close_position(ClosePositionRequest{current_qty});
        }
        StrategyLogic::ExitTargets targets = StrategyLogic::compute_exit_targets(to_side_string(OrderSide::Sell), data.curr.c, sizing.risk_amount, services.config.strategy.rr_ratio);
        if (is_short && services.config.risk.allow_multiple_positions) {
            TraderLogging::log_open_position_details("Scaling into short position", data.curr.c, targets.stop_loss, targets.take_profit, services.config);
            services.client.place_bracket_order(OrderRequest{to_side_string(OrderSide::Sell), sizing.quantity, targets.take_profit, targets.stop_loss});
        } else if (!is_long && !is_short) {
            TraderLogging::log_open_position_details("Opening new short position", data.curr.c, targets.stop_loss, targets.take_profit, services.config);
            services.client.place_bracket_order(OrderRequest{to_side_string(OrderSide::Sell), sizing.quantity, targets.take_profit, targets.stop_loss});
        } else {
            TraderLogging::log_position_limits_reached("Sell", services.config);
        }
    } else {
        TraderLogging::log_no_valid_pattern(services.config);
    }
}

void Trader::run_decision_loop() {
    set_log_thread_tag("DECIDE");
    log_message("   |  • Trader decision thread started: " + ThreadSystem::Platform::ThreadControl::get_thread_info(), "");
    
    // Wait for main thread to complete priority setup
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    decision_loop();
}

void Trader::decision_loop() {
    while (shared.running && shared.running->load()) {
        // Wait for fresh data
        wait_for_fresh_data();
        if (!shared.running->load()) break;
        
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
    if (!(shared.mtx && shared.cv && shared.has_market && shared.has_account && shared.running)) {
        TraderLogging::log_no_valid_pattern(services.config); // reuse to log an error-like line
        return;
    }
    std::unique_lock<std::mutex> lock(*shared.mtx);
    shared.cv->wait_for(lock, std::chrono::seconds(1), [&]{
        return (shared.has_market && shared.has_market->load()) && (shared.has_account && shared.has_account->load());
    });
    if (!shared.running->load()) return;
    if (!shared.has_market->load()) return;
    
    shared.has_market->store(false);
    lock.unlock();
}

std::pair<MarketSnapshot, AccountSnapshot> Trader::get_current_snapshots() {
    if (!(shared.market && shared.account)) {
        return {MarketSnapshot{}, AccountSnapshot{}};
    }
    MarketSnapshot market = *shared.market;
    AccountSnapshot account = *shared.account;
    return {market, account};
}

void Trader::display_loop_header() {
    runtime.loop_counter.fetch_add(1);
    TraderLogging::log_loop_header(runtime.loop_counter.load(), services.config);
}

void Trader::handle_trading_halt() {
    TraderLogging::log_halted_header(services.config);
    
    // Countdown while halted
    int halt_secs = services.config.timing.halt_sleep_min * 60;
    for (int s = halt_secs; s > 0 && shared.running->load(); --s) {
        log_inline_status("   ⏳ Halted: next check in " + std::to_string(s) + "s   ");
        std::this_thread::sleep_for(std::chrono::seconds(services.config.timing.countdown_tick_sec));
    }
    end_inline_status();
}

void Trader::display_equity_status(double equity) {
    TraderLogging::log_equity_status(equity, services.config);
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
    // Per-loop countdown to next cycle (visual heartbeat)
    int sleep_secs = services.config.timing.sleep_interval_sec;
    for (int s = sleep_secs; s > 0 && shared.running->load(); --s) {
        log_inline_status("   ⏳ Next loop in " + std::to_string(s) + "s   ");
        std::this_thread::sleep_for(std::chrono::seconds(services.config.timing.countdown_tick_sec));
    }
    end_inline_status();
}

void Trader::run() {
    TraderLogging::log_header_and_config(services.config);
    // In new design, main() owns market/account threads and running flag.
}

void Trader::attach_shared_state(std::mutex& mtx,
                                 std::condition_variable& cv,
                                 MarketSnapshot& market,
                                 AccountSnapshot& account,
                                 std::atomic<bool>& has_market,
                                 std::atomic<bool>& has_account,
                                 std::atomic<bool>& running_flag) {
    shared.mtx = &mtx;
    shared.cv = &cv;
    shared.market = &market;
    shared.account = &account;
    shared.has_market = &has_market;
    shared.has_account = &has_account;
    shared.running = &running_flag;
    if (!(shared.mtx && shared.cv && shared.market && shared.account && shared.has_market && shared.has_account && shared.running)) {
        TraderLogging::log_no_valid_pattern(services.config);
    }
}

void Trader::start_decision_thread() {
    // Start only decision thread
    runtime.decision_thread = std::thread([this]{ set_log_thread_tag("DECIDE"); decision_loop(); });
}

void Trader::join_decision_thread() {
    if (runtime.decision_thread.joinable()) runtime.decision_thread.join();
}
