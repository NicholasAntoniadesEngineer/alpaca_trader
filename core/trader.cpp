/**
 * Core trading engine implementation.
 * Executes trading decisions based on market signals and risk management.
 */
#include "trader.hpp"
#include "risk_logic.hpp"
#include "market_processing.hpp"
#include "../logging/async_logger.hpp"
#include "../logging/logging_macros.hpp"
#include "../threads/platform/thread_control.hpp"
#include <thread>
#include <cmath>
#include <chrono>
#include <cassert>
#include <sstream>
#include <iomanip>

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
    TradingLogger::log_startup(services.config, eq);
    return eq;
}

bool Trader::can_trade(double exposure_pct) {
    // Trading conditions will be logged by the new centralized system

    RiskLogic::TradeGateInput in;
    in.initial_equity = runtime.initial_equity;
    in.current_equity = services.account_manager.get_equity();
    in.exposure_pct = exposure_pct;
    in.core_trading_hours = services.client.is_core_trading_hours();

    RiskLogic::TradeGateResult res = RiskLogic::evaluate_trade_gate(in, services.config);

    if (!res.hours_ok) {
        TradingLogger::log_market_status(false, "Market hours restriction");
        return false;
    }

    bool trading_allowed = res.pnl_ok && res.exposure_ok;
    TradingLogger::log_trading_conditions(res.daily_pnl, exposure_pct, trading_allowed, services.config);
    
    if (!res.pnl_ok) {
        return false;
    }

    if (!res.exposure_ok) {
        return false;
    }

    TradingLogger::log_market_status(true);
    return true;
}

ProcessedData Trader::fetch_and_process_data() {
    ProcessedData data;
    // TraderLogging::log_market_data_header(services.config);

    BarRequest br{services.config.target.symbol, services.config.strategy.atr_period + services.config.timing.bar_buffer};
    auto bars = services.client.get_recent_bars(br);
    if (bars.size() < static_cast<size_t>(services.config.strategy.atr_period + 2)) {
        if (bars.size() == 0) {
            // TraderLogging::log_no_market_data(services.config);
        } else {
            // TraderLogging::log_insufficient_data(bars.size(), services.config.strategy.atr_period + 2, services.config);
        }
        // TraderLogging::log_market_data_collection_failed(services.config);
        return data;
    }

    // TraderLogging::log_computing_indicators_start(services.config);

    data = MarketProcessing::compute_processed_data(bars, services.config);
    if (data.atr == 0.0) {
        // TraderLogging::log_indicator_failure(services.config);
        return data;
    }

    // TraderLogging::log_getting_position_and_account(services.config);
    SymbolRequest sr{services.config.target.symbol};
    data.pos_details = services.account_manager.get_position_details(sr);
    data.open_orders = services.account_manager.get_open_orders_count(sr);
    double equity = services.account_manager.get_equity();
    data.exposure_pct = (equity > 0.0) ? (std::abs(data.pos_details.current_value) / equity) * 100.0 : 0.0;

    // TraderLogging::log_position_market_summary(data, services.config);

    if (data.pos_details.qty != 0 && data.open_orders == 0) {
        // TraderLogging::log_missing_bracket_warning(services.config);
    }

    return data;
}

void Trader::evaluate_and_execute_signal(const ProcessedData& data, double equity) 
{
    TradingLogger::log_signal_analysis_start(services.config.target.symbol);
    int current_qty = data.pos_details.qty;

    // Step 1: Detect signals and log candle/signal info
    StrategyLogic::SignalDecision signal_decision = detect_signals(data);
    TradingLogger::log_candle_and_signals(data, signal_decision);

    // Step 2: Evaluate filters and log details
    StrategyLogic::FilterResult filter_result = evaluate_filters(data);
    TradingLogger::log_filters(filter_result,services.config);
    TradingLogger::log_summary(data, signal_decision, filter_result);

    // Step 3: Get buying power once for efficiency
    double buying_power = services.account_manager.get_buying_power();
    
    // Early return if filters fail (with sizing preview)
    if (!filter_result.all_pass) {
        StrategyLogic::PositionSizing preview_sizing = calculate_position_sizing(data, equity, current_qty, buying_power);
        TradingLogger::log_signal_analysis_complete();
        TradingLogger::log_filters_not_met_preview(preview_sizing.risk_amount, preview_sizing.quantity);
        TradingLogger::log_position_sizing_debug(preview_sizing.risk_based_qty, preview_sizing.exposure_based_qty, preview_sizing.buying_power_qty, preview_sizing.quantity);
        return;
    }
    // TraderLogging::log_filters_pass(services.config);

    // Step 4: Calculate position sizing and validate
    TradingLogger::log_current_position(current_qty, services.config.target.symbol);
    StrategyLogic::PositionSizing sizing = calculate_position_sizing(data, equity, current_qty, buying_power);
    TradingLogger::log_position_size_with_buying_power(sizing.risk_amount, sizing.quantity, buying_power, data.curr.c);
    TradingLogger::log_position_sizing_debug(sizing.risk_based_qty, sizing.exposure_based_qty, sizing.buying_power_qty, sizing.quantity);
    
    if (sizing.quantity < 1) {
        log_message("Position sizing resulted in quantity < 1, skipping trade", "");
        return;
    }

    // Step 5: Validate trade feasibility
    if (!validate_trade_feasibility(sizing, buying_power, data.curr.c)) {
        log_message("Trade validation failed - insufficient buying power", "");
        return;
    }

    // Step 6: Execute the trade decision
    execute_trade(data, current_qty, sizing, signal_decision);

    // Final completion log
    // TraderLogging::log_signal_analysis_complete(services.config);
}

StrategyLogic::SignalDecision Trader::detect_signals(const ProcessedData& data) const {
    return StrategyLogic::detect_signals(data);
}

StrategyLogic::FilterResult Trader::evaluate_filters(const ProcessedData& data) const {
    return StrategyLogic::evaluate_filters(data, services.config);
}

StrategyLogic::PositionSizing Trader::calculate_position_sizing(const ProcessedData& data, double equity, int current_qty, double buying_power) const {
    return StrategyLogic::calculate_position_sizing(data, equity, current_qty, services.config, buying_power);
}

bool Trader::validate_trade_feasibility(const StrategyLogic::PositionSizing& sizing, double buying_power, double current_price) const {
    if (sizing.quantity <= 0) {
        return false;
    }
    
    // Calculate the total position value
    double position_value = sizing.quantity * current_price;
    
    // For conservative validation, use configurable validation factor
    // This accounts for margin requirements and market fluctuations
    double required_buying_power = position_value * services.config.risk.buying_power_validation_factor;
    
    if (buying_power < required_buying_power) {
        std::ostringstream oss;
        oss << "Insufficient buying power: Need $" << std::fixed << std::setprecision(2) << required_buying_power 
            << ", Have $" << std::fixed << std::setprecision(2) << buying_power 
            << " (Position: " << sizing.quantity << " @ $" << std::fixed << std::setprecision(2) << current_price << ")";
        log_message(oss.str(), "");
        return false;
    }
    
    return true;
}

/**
 * @brief Get best available price with realistic expectations
 * 
 * REALITY: "Free real-time data" is limited and unreliable for production trading.
 * This function attempts to get current quotes but gracefully falls back to delayed data
 * with appropriate logging to set realistic expectations.
 * 
 * @param fallback_price Delayed price from historical bars
 * @return Best available price (real-time if possible, fallback otherwise)
 */
double Trader::get_real_time_price_with_fallback(double fallback_price) const {
    // Attempt to get current quote (may fail for many symbols on free plan)
    double current_price = services.client.get_current_price(services.config.target.symbol);
    
    if (current_price <= 0.0) {
        // Expected behavior for most symbols on free IEX feed
        current_price = fallback_price;
        LOG_THREAD_CONTENT("DATA SOURCE: DELAYED BAR DATA (15-MIN DELAY) - $" + std::to_string(current_price) + " [FREE PLAN LIMITATION]");
    } else {
        LOG_THREAD_CONTENT("DATA SOURCE: IEX FREE QUOTE - $" + std::to_string(current_price) + " [LIMITED SYMBOL COVERAGE]");
    }
    
    return current_price;
}

/**
 * @brief Log exit target calculations for debugging purposes
 * @param side Order side ("BUY" or "SELL")
 * @param price Entry price used
 * @param risk Risk amount per share
 * @param rr Risk-reward ratio
 * @param targets Calculated exit targets
 */
void Trader::log_exit_target_debug(const std::string& side, double price, double risk, double rr, const StrategyLogic::ExitTargets& targets) const {
    std::ostringstream oss;
    oss << "(" << side << ") entry=$" << std::fixed << std::setprecision(3) << price 
        << " risk=$" << risk << " rr=" << rr
        << " -> SL=$" << targets.stop_loss << " TP=$" << targets.take_profit;
    LOG_THREAD_CONTENT("EXIT TARGETS: " + oss.str());
}

void Trader::execute_trade(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& sd) {
    LOG_THREAD_ORDER_EXECUTION_HEADER();
    
    bool is_long = current_qty > 0;
    bool is_short = current_qty < 0;

    if (sd.buy) {
        // TraderLogging::log_buy_triggered(services.config);
        if (is_short && services.config.risk.close_on_reverse) {
            // TraderLogging::log_close_position_first("long", services.config);
            services.client.close_position(ClosePositionRequest{current_qty});
        }
        // ATTEMPT: Get best available price (likely delayed on free plan)
        // Conservative buffers in exit target calculation handle data delays
        double current_price = get_real_time_price_with_fallback(data.curr.c);
        
        StrategyLogic::ExitTargets targets = StrategyLogic::compute_exit_targets(
            to_side_string(OrderSide::Buy), current_price, sizing.risk_amount, services.config.strategy.rr_ratio
        );
        
        // Debug logging to verify exit target calculations
        log_exit_target_debug("BUY", current_price, sizing.risk_amount, services.config.strategy.rr_ratio, targets);
        if (is_long && services.config.risk.allow_multiple_positions) {
            // TraderLogging::log_open_position_details("Scaling into long position", data.curr.c, targets.stop_loss, targets.take_profit, services.config);
            services.client.place_bracket_order(OrderRequest{to_side_string(OrderSide::Buy), sizing.quantity, targets.take_profit, targets.stop_loss});
        } else if (!is_long && !is_short) {
            // TraderLogging::log_open_position_details("Opening new long position", data.curr.c, targets.stop_loss, targets.take_profit, services.config);
            services.client.place_bracket_order(OrderRequest{to_side_string(OrderSide::Buy), sizing.quantity, targets.take_profit, targets.stop_loss});
        } else {
            // TraderLogging::log_position_limits_reached("Buy", services.config);
        }
    } else if (sd.sell) {
        // TraderLogging::log_sell_triggered(services.config);
        if (is_long && services.config.risk.close_on_reverse) {
            // TraderLogging::log_close_position_first("short", services.config);
            services.client.close_position(ClosePositionRequest{current_qty});
        }
        // ATTEMPT: Get best available price (likely delayed on free plan)
        double current_price = get_real_time_price_with_fallback(data.curr.c);
        
        StrategyLogic::ExitTargets targets = StrategyLogic::compute_exit_targets(
            to_side_string(OrderSide::Sell), current_price, sizing.risk_amount, services.config.strategy.rr_ratio
        );
        
        // Debug logging to verify exit target calculations
        log_exit_target_debug("SELL", current_price, sizing.risk_amount, services.config.strategy.rr_ratio, targets);
        if (is_short && services.config.risk.allow_multiple_positions) {
            // TraderLogging::log_open_position_details("Scaling into short position", data.curr.c, targets.stop_loss, targets.take_profit, services.config);
            services.client.place_bracket_order(OrderRequest{to_side_string(OrderSide::Sell), sizing.quantity, targets.take_profit, targets.stop_loss});
        } else if (!is_long && !is_short) {
            // TraderLogging::log_open_position_details("Opening new short position", data.curr.c, targets.stop_loss, targets.take_profit, services.config);
            services.client.place_bracket_order(OrderRequest{to_side_string(OrderSide::Sell), sizing.quantity, targets.take_profit, targets.stop_loss});
        } else {
            // TraderLogging::log_position_limits_reached("Sell", services.config);
        }
    } else {
        // TraderLogging::log_no_valid_pattern(services.config);
    }
}

void Trader::run_decision_loop() {
    set_log_thread_tag("DECIDE");
    log_message("   |  • Trader decision thread started: " + ThreadSystem::Platform::ThreadControl::get_thread_info(), "");
    
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
        
        // Log trading loop start
        display_loop_header();
        
        // Check if we can trade
        if (!can_trade(account.exposure_pct)) {
            handle_trading_halt();
            continue;
        }
        
        // Log current equity status
        display_equity_status(account.equity);
        
        // Process data and execute signals
        process_trading_cycle(market, account);
        
        // Increment iteration counter for monitoring
        if (runtime.iteration_counter) {
            runtime.iteration_counter->fetch_add(1);
        }
        
        // Countdown to next cycle
        countdown_to_next_cycle();
    }
}

void Trader::wait_for_fresh_data() {
    if (!(shared.mtx && shared.cv && shared.has_market && shared.has_account && shared.running)) {
        // TraderLogging::log_no_valid_pattern(services.config); // reuse to log an error-like line
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
    TradingLogger::log_loop_header(runtime.loop_counter.load(), services.config.target.symbol);
}

void Trader::handle_trading_halt() {    
    // Countdown while halted
    int halt_secs = services.config.timing.halt_sleep_min * 60;
    for (int s = halt_secs; s > 0 && shared.running->load(); --s) {
        LOG_INLINE_HALT_STATUS(s);
        std::this_thread::sleep_for(std::chrono::seconds(services.config.timing.countdown_tick_sec));
    }
    end_inline_status();
}

void Trader::display_equity_status(double /* equity */) {
    // TraderLogging::log_equity_status(equity, services.config);
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
    // Countdown timer to next trading cycle
    int sleep_secs = services.config.timing.sleep_interval_sec;
    for (int s = sleep_secs; s > 0 && shared.running->load(); --s) {
        LOG_INLINE_NEXT_LOOP(s);
        std::this_thread::sleep_for(std::chrono::seconds(services.config.timing.countdown_tick_sec));
    }
    end_inline_status();
}

void Trader::run() {
    TradingLogger::log_header_and_config(services.config);
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
        // TraderLogging::log_no_valid_pattern(services.config);
    }
}

void Trader::start_decision_thread() {
    // Start only decision thread
    runtime.decision_thread = std::thread([this]{ set_log_thread_tag("DECIDE"); decision_loop(); });
}

void Trader::join_decision_thread() {
    if (runtime.decision_thread.joinable()) runtime.decision_thread.join();
}

void Trader::set_iteration_counter(std::atomic<unsigned long>& counter) {
    runtime.iteration_counter = &counter;
}
