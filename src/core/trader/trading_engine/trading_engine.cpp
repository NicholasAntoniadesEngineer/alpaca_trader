#include "trading_engine.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/system/system_monitor.hpp"
#include "core/utils/time_utils.hpp"
#include "api/general/api_manager.hpp"
#include <chrono>
#include <cmath>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

TradingEngine::TradingEngine(const SystemConfig& cfg, API::ApiManager& api_mgr, AccountManager& account_mgr)
    : config(cfg), account_manager(account_mgr), api_manager(api_mgr),
      risk_manager(cfg),
      signal_processor(cfg),
      order_engine(api_mgr, account_mgr, cfg, data_sync),
      data_fetcher(api_mgr, account_mgr, cfg) {}

void TradingEngine::execute_trading_decision(const ProcessedData& data, double equity) {
    // Input validation
    if (config.trading_mode.primary_symbol.empty()) {
        TradingLogs::log_market_status(false, "Invalid configuration - primary symbol is empty");
        return;
    }

    if (equity <= 0.0 || !std::isfinite(equity)) {
        TradingLogs::log_market_status(false, "Invalid equity value - must be positive and finite");
        return;
    }

    TradingLogs::log_signal_analysis_start(config.trading_mode.primary_symbol);

    // Check if market is open before making any trading decisions
    if (!data_fetcher.get_session_manager().is_market_open()) {
        TradingLogs::log_market_status(false, "Market is closed - no trading decisions");
        TradingLogs::log_signal_analysis_complete();
        return;
    }
    
    // Check if market data is fresh enough for trading decisions
    if (!data_fetcher.is_data_fresh()) {
        TradingLogs::log_market_status(false, "Market data is stale - no trading decisions");
        AlpacaTrader::Core::Monitoring::SystemMonitor::instance().record_data_freshness_failure();
        TradingLogs::log_signal_analysis_complete();
        return;
    }
    
    int current_qty = data.pos_details.qty;

    // Check for profit-taking opportunity first
    if (current_qty != 0 && config.strategy.profit_taking_threshold_dollars > 0.0) {
        check_and_execute_profit_taking(data, current_qty);
    }

    // Process signal analysis
    signal_processor.process_signal_analysis(data);

    // Process position sizing and execute if valid
    double buying_power = account_manager.fetch_buying_power();
    auto [sizing, signal_decision] = signal_processor.process_position_sizing(data, equity, current_qty, buying_power);
    execute_trade_if_valid(data, current_qty, sizing, signal_decision);
    
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
        halt_seconds = config.timing.emergency_trading_halt_duration_minutes * 60;
    }
    
    perform_halt_countdown(halt_seconds);
    TradingLogs::end_inline_status();
}

void TradingEngine::execute_trade_if_valid(const ProcessedData& data, int current_qty, const PositionSizing& sizing, const SignalDecision& signal_decision) {
    if (sizing.quantity < 1) {
        TradingLogs::log_position_sizing_skipped("quantity < 1");
        return;
    }
    
    double buying_power = account_manager.fetch_buying_power();
    if (!order_engine.validate_trade_feasibility(sizing, buying_power, data.curr.c)) {
        TradingLogs::log_trade_validation_failed("insufficient buying power");
        return;
    }
    
    order_engine.execute_trade(data, current_qty, sizing, signal_decision);
}

void TradingEngine::perform_halt_countdown(int seconds) const {
    for (int s = seconds; s > 0; --s) {
        TradingLogs::log_inline_halt_status(s);
        std::this_thread::sleep_for(std::chrono::seconds(config.timing.countdown_display_refresh_interval_seconds));
    }
}

void TradingEngine::check_and_execute_profit_taking(const ProcessedData& data, int current_qty) {
    // Get unrealized profit/loss from position details
    double unrealized_pl = data.pos_details.unrealized_pl;
    double current_price = data.curr.c;
    double position_value = data.pos_details.current_value;
    
    // Log profit calculation using proper TradingLogs class
    TradingLogs::log_position_sizing_debug(current_qty, position_value, current_qty, true, false);
    TradingLogs::log_exit_targets_table("LONG", current_price, config.strategy.profit_taking_threshold_dollars, config.strategy.rr_ratio, 0.0, 0.0);

    // Check if profit exceeds threshold
    if (unrealized_pl > config.strategy.profit_taking_threshold_dollars) {
        TradingLogs::log_position_closure("PROFIT TAKING THRESHOLD EXCEEDED", abs(current_qty));
        TradingLogs::log_comprehensive_order_execution("MARKET", "SELL", abs(current_qty), current_price, 0.0, current_qty, config.strategy.profit_taking_threshold_dollars);
        
        // Execute market order to close entire position
        if (current_qty > 0) {
            // Long position: sell to close
            order_engine.execute_market_order(OrderExecutionEngine::OrderSide::Sell, data, 
PositionSizing{abs(current_qty), 0.0, 0.0, static_cast<int>(0)});
        } else {
            // Short position: buy to close
            order_engine.execute_market_order(OrderExecutionEngine::OrderSide::Buy, data, 
PositionSizing{abs(current_qty), 0.0, 0.0, static_cast<int>(0)});
        }
    }
}

} // namespace Core
} // namespace AlpacaTrader
