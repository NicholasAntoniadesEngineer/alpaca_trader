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
    if (!check_market_conditions()) {
        return false;
    }
    
    if (!check_connectivity()) {
        return false;
    }
    
    return validate_risk_conditions(data, equity);
}

void TradingEngine::execute_trading_decision(const ProcessedData& data, double equity) {
    TradingLogs::log_signal_analysis_start(config.target.symbol);
    
    int current_qty = data.pos_details.qty;
    
    StrategyLogic::SignalDecision signal_decision = StrategyLogic::detect_trading_signals(data, config);
    TradingLogs::log_candle_and_signals(data, signal_decision);
    
    StrategyLogic::FilterResult filter_result = StrategyLogic::evaluate_trading_filters(data, config);
    TradingLogs::log_filters(filter_result, config);
    TradingLogs::log_summary(data, signal_decision, filter_result, config.target.symbol);
    
    double buying_power = account_manager.fetch_buying_power();
    
    if (!filter_result.all_pass) {
        StrategyLogic::PositionSizing preview_sizing = StrategyLogic::calculate_position_sizing(data, equity, current_qty, config, buying_power);
        TradingLogs::log_signal_analysis_complete();
        TradingLogs::log_filters_not_met_preview(preview_sizing.risk_amount, preview_sizing.quantity);
        return;
    }
    
    TradingLogs::log_filters_passed();
    
    TradingLogs::log_current_position(current_qty, config.target.symbol);
    StrategyLogic::PositionSizing sizing = StrategyLogic::calculate_position_sizing(data, equity, current_qty, config, buying_power);
    TradingLogs::log_position_size_with_buying_power(sizing.risk_amount, sizing.quantity, buying_power, data.curr.c);
    TradingLogs::log_position_sizing_debug(sizing.risk_based_qty, sizing.exposure_based_qty, sizing.max_value_qty, sizing.buying_power_qty, sizing.quantity);
    
    if (sizing.quantity < 1) {
        TradingLogs::log_position_sizing_skipped("quantity < 1");
        return;
    }
    
    if (!trade_validator.validate_trade_feasibility(sizing, buying_power, data.curr.c)) {
        TradingLogs::log_trade_validation_failed("insufficient buying power");
        return;
    }
    
    order_engine.execute_trade(data, current_qty, sizing, signal_decision);
    TradingLogs::log_signal_analysis_complete();
}

void TradingEngine::handle_trading_halt(const std::string& reason) {
    TradingLogs::log_market_status(false, reason);
    
    auto& connectivity = ConnectivityManager::instance();
    
    if (connectivity.is_connectivity_outage()) {
        int retry_secs = connectivity.get_seconds_until_retry();
        if (retry_secs > 0) {
            for (int s = retry_secs; s > 0; --s) {
                TradingLogs::log_inline_halt_status(s);
                std::this_thread::sleep_for(std::chrono::seconds(config.timing.countdown_tick_sec));
            }
        } else {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } else {
        int halt_secs = config.timing.halt_sleep_min * 60;
        for (int s = halt_secs; s > 0; --s) {
            TradingLogs::log_inline_halt_status(s);
            std::this_thread::sleep_for(std::chrono::seconds(config.timing.countdown_tick_sec));
        }
    }
    TradingLogs::end_inline_status();
}

void TradingEngine::process_trading_cycle(const MarketSnapshot& market, const AccountSnapshot& account) {
    // Validate market data before processing
    if (market.curr.c <= 0.0 || market.atr <= 0.0) {
        TradingLogs::log_market_status(false, "Invalid market data - price or ATR is zero");
        return;
    }
    
    ProcessedData pd;
    pd.atr = market.atr;
    pd.avg_atr = market.avg_atr;
    pd.avg_vol = market.avg_vol;
    pd.curr = market.curr;
    pd.prev = market.prev;
    pd.pos_details = account.pos_details;
    pd.open_orders = account.open_orders;
    pd.exposure_pct = account.exposure_pct;
    
    position_manager.handle_market_close_positions(pd);
    execute_trading_decision(pd, account.equity);
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

void TradingEngine::process_trading_signals(const ProcessedData& data, double equity) {
    execute_trading_decision(data, equity);
}

bool TradingEngine::check_market_conditions() {
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

void TradingEngine::log_trading_conditions(bool allowed, const std::string& reason) {
    if (allowed) {
        TradingLogs::log_market_status(true);
    } else {
        TradingLogs::log_market_status(false, reason);
    }
}

} // namespace Core
} // namespace AlpacaTrader
