#include "order_execution_engine.hpp"
#include "core/logging/trading_logs.hpp"
#include <thread>
#include <chrono>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

OrderExecutionEngine::OrderExecutionEngine(API::AlpacaClient& client_ref, AccountManager& account_mgr, const TraderConfig& cfg)
    : client(client_ref), account_manager(account_mgr), config(cfg) {}

void OrderExecutionEngine::execute_trade(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& sd) {
    TradingLogs::log_order_execution_header();
    
    bool is_long = current_qty > 0;
    bool is_short = current_qty < 0;
    
    TradingLogs::log_debug_position_data(current_qty, data.pos_details.current_value, data.pos_details.qty, is_long, is_short);

    if (sd.buy) {
        TradingLogs::log_signal_triggered("BUY", true);
        execute_buy_order(data, current_qty, sizing);
    } else if (sd.sell) {
        TradingLogs::log_signal_triggered("SELL", true);
        execute_sell_order(data, current_qty, sizing);
    } else {
        TradingLogs::log_no_trading_pattern();
    }
}

void OrderExecutionEngine::execute_buy_order(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing) {
    if (current_qty < 0 && config.risk.close_on_reverse) {
        TradingLogs::log_position_closure("Closing short position first for long signal", current_qty);
        client.close_position(ClosePositionRequest{current_qty});
    }
    
    double current_price = data.curr.c;
    StrategyLogic::ExitTargets targets = StrategyLogic::compute_exit_targets(
        to_side_string(OrderSide::Buy), current_price, sizing.risk_amount, config.strategy.rr_ratio, config
    );
    
    TradingLogs::log_exit_targets_table("BUY", current_price, sizing.risk_amount, config.strategy.rr_ratio, targets.stop_loss, targets.take_profit);
    
    bool is_long = current_qty > 0;
    bool is_short = current_qty < 0;
    
    if (is_long && config.risk.allow_multiple_positions) {
        TradingLogs::log_order_intent("BUY", data.curr.c, targets.stop_loss, targets.take_profit);
        client.place_bracket_order(OrderRequest{to_side_string(OrderSide::Buy), sizing.quantity, targets.take_profit, targets.stop_loss});
    } else if (!is_long && !is_short) {
        TradingLogs::log_order_intent("BUY", data.curr.c, targets.stop_loss, targets.take_profit);
        client.place_bracket_order(OrderRequest{to_side_string(OrderSide::Buy), sizing.quantity, targets.take_profit, targets.stop_loss});
    } else if (is_short && config.risk.allow_multiple_positions) {
        handle_multiple_position_logic(OrderSide::Buy, data, sizing, targets);
    } else {
        TradingLogs::log_position_limits_reached("Buy");
    }
}

void OrderExecutionEngine::execute_sell_order(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing) {
    if (current_qty > 0 && config.risk.close_on_reverse) {
        TradingLogs::log_position_closure("Closing long position first for short signal", current_qty);
        client.close_position(ClosePositionRequest{current_qty});
    }
    
    double current_price = data.curr.c;
    StrategyLogic::ExitTargets targets = StrategyLogic::compute_exit_targets(
        to_side_string(OrderSide::Sell), current_price, sizing.risk_amount, config.strategy.rr_ratio, config
    );
    
    TradingLogs::log_exit_targets_table("SELL", current_price, sizing.risk_amount, config.strategy.rr_ratio, targets.stop_loss, targets.take_profit);
    
    bool is_long = current_qty > 0;
    bool is_short = current_qty < 0;
    
    if (is_short && config.risk.allow_multiple_positions) {
        TradingLogs::log_order_intent("SELL", data.curr.c, targets.stop_loss, targets.take_profit);
        client.place_bracket_order(OrderRequest{to_side_string(OrderSide::Sell), sizing.quantity, targets.take_profit, targets.stop_loss});
    } else if (!is_long && !is_short) {
        TradingLogs::log_order_intent("SELL", data.curr.c, targets.stop_loss, targets.take_profit);
        client.place_bracket_order(OrderRequest{to_side_string(OrderSide::Sell), sizing.quantity, targets.take_profit, targets.stop_loss});
    } else if (is_long && config.risk.allow_multiple_positions) {
        handle_multiple_position_logic(OrderSide::Sell, data, sizing, targets);
    } else {
        TradingLogs::log_position_limits_reached("Sell");
    }
}

void OrderExecutionEngine::handle_multiple_position_logic(OrderSide side, const ProcessedData& data, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::ExitTargets& targets) {
    std::string side_str = (side == OrderSide::Buy) ? "BUY" : "SELL";
    std::string opposite_side_str = (side == OrderSide::Buy) ? "SHORT" : "LONG";
    
    TradingLogs::log_debug_fresh_data_fetch(opposite_side_str);
    
    AccountSnapshot fresh_account = account_manager.fetch_account_snapshot();
    int fresh_qty = fresh_account.pos_details.qty;
    
    TradingLogs::log_debug_fresh_position_data(fresh_qty, data.pos_details.qty);
    TradingLogs::log_debug_account_details(fresh_account.pos_details.qty, fresh_account.pos_details.current_value);
    
    bool has_opposite_position = (side == OrderSide::Buy) ? (fresh_qty < 0) : (fresh_qty > 0);
    
    if (has_opposite_position) {
        TradingLogs::log_debug_position_closure_attempt(fresh_qty);
        client.close_position(ClosePositionRequest{fresh_qty});
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        AccountSnapshot verify_account = account_manager.fetch_account_snapshot();
        int verify_qty = verify_account.pos_details.qty;
        TradingLogs::log_debug_position_verification(verify_qty);
        
        if (verify_qty == 0) {
            TradingLogs::log_order_intent(side_str, data.curr.c, targets.stop_loss, targets.take_profit);
            client.place_bracket_order(OrderRequest{to_side_string(side), sizing.quantity, targets.take_profit, targets.stop_loss});
        } else {
            TradingLogs::log_debug_position_still_exists(side_str);
        }
    } else {
        TradingLogs::log_debug_no_position_found(opposite_side_str);
        TradingLogs::log_order_intent(side_str, data.curr.c, targets.stop_loss, targets.take_profit);
        client.place_bracket_order(OrderRequest{to_side_string(side), sizing.quantity, targets.take_profit, targets.stop_loss});
    }
}

} // namespace Core
} // namespace AlpacaTrader
