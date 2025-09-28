#include "trading_engine.hpp"
#include "../analysis/risk_logic.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/logging_macros.hpp"

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

TradingEngine::TradingEngine(const TraderConfig& cfg, API::AlpacaClient& client_ref, AccountManager& account_mgr)
    : config(cfg), account_manager(account_mgr),
      order_engine(client_ref, account_mgr, cfg),
      position_manager(client_ref, cfg),
      trade_validator(cfg),
      price_manager(client_ref, cfg),
      data_fetcher(client_ref, account_mgr, cfg) {}

bool TradingEngine::check_trading_permissions(const ProcessedData& data, double equity) {
    if (!check_connectivity()) {
        return false;
    }
    
    return validate_risk_conditions(data, equity);
}

void TradingEngine::execute_trading_decision(const ProcessedData& data, double equity) {
    TradingLogs::log_signal_analysis_start(config.target.symbol);
    
    int current_qty = data.pos_details.qty;
    
    // Process signal analysis
    process_signal_analysis(data, equity);
    
    // Process position sizing and execute if valid
    process_position_sizing(data, equity, current_qty);
    
    TradingLogs::log_signal_analysis_complete();
}

void TradingEngine::handle_trading_halt(const std::string& reason) {
    TradingLogs::log_market_status(false, reason);
    
    auto& connectivity = ConnectivityManager::instance();
    
    int halt_seconds = 0;
    if (connectivity.is_connectivity_outage()) {
        halt_seconds = connectivity.get_seconds_until_retry();
        if (halt_seconds <= 0) {
            halt_seconds = CONNECTIVITY_RETRY_SECONDS;
        }
    } else {
        halt_seconds = config.timing.halt_sleep_min * 60;
    }
    
    perform_halt_countdown(halt_seconds);
    TradingLogs::end_inline_status();
}

bool TradingEngine::validate_risk_conditions(const ProcessedData& data, double equity) {
    RiskLogic::TradeGateInput in;
    in.initial_equity = 0.0;
    in.current_equity = equity;
    in.exposure_pct = data.exposure_pct;
    
    RiskLogic::TradeGateResult res = RiskLogic::evaluate_trade_gate(in, config);
    
    bool trading_allowed = res.pnl_ok && res.exposure_ok;
    TradingLogs::log_trading_conditions(res.daily_pnl, data.exposure_pct, trading_allowed, config);
    
    if (!res.pnl_ok || !res.exposure_ok) {
        return false;
    }
    
    TradingLogs::log_market_status(true);
    return true;
}

bool TradingEngine::validate_market_data(const MarketSnapshot& market) const {
    if (market.curr.c <= 0.0 || market.atr <= 0.0) {
        TradingLogs::log_market_status(false, "Invalid market data - price or ATR is zero");
        return false;
    }
    return true;
}

bool TradingEngine::check_connectivity() {
    auto& connectivity = ConnectivityManager::instance();
    if (connectivity.is_connectivity_outage()) {
        std::string connectivity_msg = "Connectivity outage - status: " + connectivity.get_status_string();
        TradingLogs::log_market_status(false, connectivity_msg);
        return false;
    }
    return true;
}

void TradingEngine::process_signal_analysis(const ProcessedData& data, double /*equity*/) {
    StrategyLogic::SignalDecision signal_decision = StrategyLogic::detect_trading_signals(data, config);
    TradingLogs::log_candle_and_signals(data, signal_decision);
    
    StrategyLogic::FilterResult filter_result = StrategyLogic::evaluate_trading_filters(data, config);
    TradingLogs::log_filters(filter_result, config);
    TradingLogs::log_summary(data, signal_decision, filter_result, config.target.symbol);
}

void TradingEngine::process_position_sizing(const ProcessedData& data, double equity, int current_qty) {
    double buying_power = account_manager.fetch_buying_power();
    StrategyLogic::PositionSizing sizing = StrategyLogic::calculate_position_sizing(data, equity, current_qty, config, buying_power);
    
    if (!StrategyLogic::evaluate_trading_filters(data, config).all_pass) {
        TradingLogs::log_filters_not_met_preview(sizing.risk_amount, sizing.quantity);
        return;
    }
    
    TradingLogs::log_filters_passed();
    TradingLogs::log_current_position(current_qty, config.target.symbol);
    TradingLogs::log_position_size_with_buying_power(sizing.risk_amount, sizing.quantity, buying_power, data.curr.c);
    TradingLogs::log_position_sizing_debug(sizing.risk_based_qty, sizing.exposure_based_qty, sizing.max_value_qty, sizing.buying_power_qty, sizing.quantity);
    
    StrategyLogic::SignalDecision signal_decision = StrategyLogic::detect_trading_signals(data, config);
    execute_trade_if_valid(data, current_qty, sizing, signal_decision);
}

void TradingEngine::execute_trade_if_valid(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& signal_decision) {
    if (sizing.quantity < 1) {
        TradingLogs::log_position_sizing_skipped("quantity < 1");
        return;
    }
    
    double buying_power = account_manager.fetch_buying_power();
    if (!trade_validator.validate_trade_feasibility(sizing, buying_power, data.curr.c)) {
        TradingLogs::log_trade_validation_failed("insufficient buying power");
        return;
    }
    
    order_engine.execute_trade(data, current_qty, sizing, signal_decision);
}

void TradingEngine::perform_halt_countdown(int seconds) const {
    for (int s = seconds; s > 0; --s) {
        TradingLogs::log_inline_halt_status(s);
        std::this_thread::sleep_for(std::chrono::seconds(config.timing.countdown_tick_sec));
    }
}

void TradingEngine::handle_market_close_positions(const ProcessedData& data) {
    position_manager.handle_market_close_positions(data);
}

} // namespace Core
} // namespace AlpacaTrader
