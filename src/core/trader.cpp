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
    TraderLogging::log_trader_started(config, eq);
    return eq;
}

bool Trader::can_trade(double exposure_pct) {
    const TraderConfig& config = *config_ptr;
    TraderLogging::log_trading_conditions_start(config);

    RiskLogic::TradeGateInput in;
    in.initial_equity = initial_equity;
    in.current_equity = account_manager.get_equity();
    in.exposure_pct = exposure_pct;
    in.core_trading_hours = client.is_core_trading_hours();

    RiskLogic::TradeGateResult res = RiskLogic::evaluate_trade_gate(in, config);

    if (!res.hours_ok) {
        TraderLogging::log_market_closed(config);
        TraderLogging::log_trading_halted_tail(config);
        return false;
    }

    TraderLogging::log_daily_pnl_line(res.daily_pnl, config);
    if (!res.pnl_ok) {
        TraderLogging::log_pnl_limit_reached(config);
        TraderLogging::log_trading_halted_tail(config);
        return false;
    }

    TraderLogging::log_exposure_line(exposure_pct, config);
    if (!res.exposure_ok) {
        TraderLogging::log_exposure_limit_reached(config);
        TraderLogging::log_trading_halted_tail(config);
        return false;
    }

    TraderLogging::log_trading_allowed(config);
    return true;
}

ProcessedData Trader::fetch_and_process_data() {
    const TraderConfig& config = *config_ptr;
    ProcessedData data;
    TraderLogging::log_market_data_header(config);

    BarRequest br{config.target.symbol, config.strategy.atr_period + config.timing.bar_buffer};
    auto bars = client.get_recent_bars(br);
    if (bars.size() < static_cast<size_t>(config.strategy.atr_period + 2)) {
        if (bars.size() == 0) {
            TraderLogging::log_no_market_data(config);
        } else {
            TraderLogging::log_insufficient_data(bars.size(), config.strategy.atr_period + 2, config);
        }
        TraderLogging::log_market_data_collection_failed(config);
        return data;
    }

    TraderLogging::log_computing_indicators_start(config);

    data = MarketProcessing::compute_processed_data(bars, config);
    if (data.atr == 0.0) {
        TraderLogging::log_indicator_failure(config);
        return data;
    }

    TraderLogging::log_getting_position_and_account(config);
    SymbolRequest sr{config.target.symbol};
    data.pos_details = account_manager.get_position_details(sr);
    data.open_orders = account_manager.get_open_orders_count(sr);
    double equity = account_manager.get_equity();
    data.exposure_pct = (equity > 0.0) ? (std::abs(data.pos_details.current_value) / equity) * 100.0 : 0.0;

    TraderLogging::log_position_market_summary(data, config);

    if (data.pos_details.qty != 0 && data.open_orders == 0) {
        TraderLogging::log_missing_bracket_warning(config);
    }

    return data;
}

void Trader::evaluate_and_execute_signal(const ProcessedData& data, double equity) {
    const TraderConfig& config = *config_ptr;
    TraderLogging::log_signal_analysis_start(config);

    int current_qty = data.pos_details.qty;

    // Step 1: Detect signals and log candle/signal info
    StrategyLogic::SignalDecision signal_decision = detect_signals(data);
    TraderLogging::log_candle_and_signals(data, signal_decision, config);

    // Step 2: Evaluate filters and log details
    StrategyLogic::FilterResult filter_result = evaluate_filters(data);
    TraderLogging::log_filters(filter_result, config);
    TraderLogging::log_summary(data, signal_decision, filter_result, config);

    // Step 3: Early return if filters fail (with sizing preview)
    if (!filter_result.all_pass) {
        double risk_prev = data.atr > 0.0 ? data.atr : 1.0;
        int qty_prev = static_cast<int>(std::floor((equity * config.risk.risk_per_trade) / risk_prev));
        TraderLogging::log_filters_not_met_preview(risk_prev, qty_prev, config);
        return;
    }
    TraderLogging::log_filters_pass(config);

    if (current_qty != 0) {
        TraderLogging::log_current_position(current_qty, config);
    }

    // Step 4: Calculate position sizing and validate
    StrategyLogic::PositionSizing sizing = calculate_position_sizing(data, equity, current_qty);
    TraderLogging::log_position_size(sizing.risk_amount, sizing.quantity, config);
    if (sizing.quantity < 1) {
        TraderLogging::log_qty_too_small(config);
        return;
    }

    // Step 5: Execute the trade decision
    execute_trade(data, current_qty, sizing, signal_decision);

    TraderLogging::log_signal_analysis_complete(config);
}

StrategyLogic::SignalDecision Trader::detect_signals(const ProcessedData& data) const {
    return StrategyLogic::detect_signals(data);
}

StrategyLogic::FilterResult Trader::evaluate_filters(const ProcessedData& data) const {
    const TraderConfig& config = *config_ptr;
    return StrategyLogic::evaluate_filters(data, config);
}

StrategyLogic::PositionSizing Trader::calculate_position_sizing(const ProcessedData& data, double equity, int current_qty) const {
    const TraderConfig& config = *config_ptr;
    return StrategyLogic::calculate_position_sizing(data, equity, current_qty, config);
}

void Trader::execute_trade(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& sd) {
    const TraderConfig& config = *config_ptr;
    bool is_long = current_qty > 0;
    bool is_short = current_qty < 0;

    if (sd.buy) {
        TraderLogging::log_buy_triggered(config);
        if (is_short && config.risk.close_on_reverse) {
            TraderLogging::log_close_position_first("long", config);
            client.close_position(ClosePositionRequest{current_qty});
        }
        StrategyLogic::ExitTargets targets = StrategyLogic::compute_exit_targets("buy", data.curr.c, sizing.risk_amount, config.strategy.rr_ratio);
        if (is_long && config.risk.allow_multiple_positions) {
            TraderLogging::log_open_position_details("Scaling into long position", data.curr.c, targets.stop_loss, targets.take_profit, config);
            client.place_bracket_order(OrderRequest{"buy", sizing.quantity, targets.take_profit, targets.stop_loss});
        } else if (!is_long && !is_short) {
            TraderLogging::log_open_position_details("Opening new long position", data.curr.c, targets.stop_loss, targets.take_profit, config);
            client.place_bracket_order(OrderRequest{"buy", sizing.quantity, targets.take_profit, targets.stop_loss});
        } else {
            TraderLogging::log_position_limits_reached("Buy", config);
        }
    } else if (sd.sell) {
        TraderLogging::log_sell_triggered(config);
        if (is_long && config.risk.close_on_reverse) {
            TraderLogging::log_close_position_first("short", config);
            client.close_position(ClosePositionRequest{current_qty});
        }
        StrategyLogic::ExitTargets targets = StrategyLogic::compute_exit_targets("sell", data.curr.c, sizing.risk_amount, config.strategy.rr_ratio);
        if (is_short && config.risk.allow_multiple_positions) {
            TraderLogging::log_open_position_details("Scaling into short position", data.curr.c, targets.stop_loss, targets.take_profit, config);
            client.place_bracket_order(OrderRequest{"sell", sizing.quantity, targets.take_profit, targets.stop_loss});
        } else if (!is_long && !is_short) {
            TraderLogging::log_open_position_details("Opening new short position", data.curr.c, targets.stop_loss, targets.take_profit, config);
            client.place_bracket_order(OrderRequest{"sell", sizing.quantity, targets.take_profit, targets.stop_loss});
        } else {
            TraderLogging::log_position_limits_reached("Sell", config);
        }
    } else {
        TraderLogging::log_no_valid_pattern(config);
    }
}

void Trader::run_decision_loop() {
    set_log_thread_tag("DECIDE");
    decision_loop();
}

void Trader::decision_loop() {
    while (running_ptr && running_ptr->load()) {
        wait_for_fresh_data();
        if (!running_ptr->load()) break;
        
        auto snapshots = get_current_snapshots();
        MarketSnapshot market = snapshots.first;
        AccountSnapshot account = snapshots.second;
        
        display_loop_header();
        
        if (!can_trade(account.exposure_pct)) {
            handle_trading_halt();
            continue;
        }
        
        display_equity_status(account.equity);
        
        process_trading_cycle(market, account);
        
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
    TraderLogging::log_loop_header(loop_counter.load(), config);
}

void Trader::handle_trading_halt() {
    const TraderConfig& config = *config_ptr;
    TraderLogging::log_halted_header(config);
    
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
    TraderLogging::log_equity_status(equity, config);
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
    const TraderConfig& config = *config_ptr;
    TraderLogging::log_header_and_config(config);
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
