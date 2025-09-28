#include "order_execution_engine.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/trader/data/data_structures.hpp"
#include <thread>
#include <chrono>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

OrderExecutionEngine::OrderExecutionEngine(API::AlpacaClient& client_ref, AccountManager& account_mgr, const TraderConfig& cfg)
    : client(client_ref), account_manager(account_mgr), config(cfg) {}

void OrderExecutionEngine::execute_trade(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& sd) {
    TradingLogs::log_order_execution_header();
    
    if (!validate_order_parameters(data, sizing)) {
        return;
    }
    
    bool is_long = is_long_position(current_qty);
    bool is_short = is_short_position(current_qty);
    TradingLogs::log_debug_position_data(current_qty, 0.0, current_qty, is_long, is_short);

    if (sd.buy) {
        TradingLogs::log_signal_triggered(SIGNAL_BUY, true);
        execute_order(OrderSide::Buy, data, current_qty, sizing);
    } else if (sd.sell) {
        TradingLogs::log_signal_triggered(SIGNAL_SELL, true);
        execute_order(OrderSide::Sell, data, current_qty, sizing);
    } else {
        TradingLogs::log_no_trading_pattern();
    }
}

// Core execution method - unified for both buy and sell orders
void OrderExecutionEngine::execute_order(OrderSide side, const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing) {
    TradingLogs::log_debug_position_data(current_qty, data.pos_details.current_value, data.pos_details.qty, is_long_position(current_qty), is_short_position(current_qty));
    
    // Handle opposite position closure if required
    if (should_close_opposite_position(side, current_qty)) {
        if (!close_opposite_position(side, current_qty)) {
            TradingLogs::log_position_limits_reached(to_side_string(side));
            return;
        }
    }
    
    // Check if we can execute a new position
    if (!can_execute_new_position(side, current_qty)) {
        TradingLogs::log_position_limits_reached(to_side_string(side));
        return;
    }
    
    // Calculate exit targets and execute bracket order
    StrategyLogic::ExitTargets targets = calculate_exit_targets(side, data, sizing);
    execute_bracket_order(side, data, sizing, targets);
}

// Execute bracket order with proper validation
void OrderExecutionEngine::execute_bracket_order(OrderSide side, const ProcessedData& data, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::ExitTargets& targets) {
    std::string side_str = to_side_string(side);
    
    TradingLogs::log_exit_targets_table(side_str, data.curr.c, sizing.risk_amount, config.strategy.rr_ratio, targets.stop_loss, targets.take_profit);
    TradingLogs::log_order_intent(side_str, data.curr.c, targets.stop_loss, targets.take_profit);
    
    try {
        client.place_bracket_order(OrderRequest{side_str, sizing.quantity, targets.take_profit, targets.stop_loss});
    } catch (const std::exception& e) {
        TradingLogs::log_market_status(false, "Order execution failed: " + std::string(e.what()));
    }
}

// Position management methods
bool OrderExecutionEngine::should_close_opposite_position(OrderSide side, int current_qty) const {
    if (!config.risk.close_on_reverse) {
        return false;
    }
    
    return (side == OrderSide::Buy && is_short_position(current_qty)) ||
           (side == OrderSide::Sell && is_long_position(current_qty));
}

bool OrderExecutionEngine::close_opposite_position(OrderSide side, int current_qty) {
    std::string side_str = to_side_string(side);
    std::string opposite_side_str = to_opposite_side_string(side);
    
    TradingLogs::log_position_closure("Closing " + opposite_side_str + " position first for " + side_str + " signal", current_qty);
    
    try {
        client.close_position(ClosePositionRequest{current_qty});
        
        // Wait for position closure and verify
        std::this_thread::sleep_for(POSITION_CLOSE_WAIT_TIME);
        
        for (int attempt = 0; attempt < MAX_POSITION_VERIFICATION_ATTEMPTS; ++attempt) {
            AccountSnapshot verify_account = account_manager.fetch_account_snapshot();
            int verify_qty = verify_account.pos_details.qty;
            
            if (verify_qty == 0) {
                TradingLogs::log_debug_position_verification(verify_qty);
                return true;
            }
            
            if (attempt < MAX_POSITION_VERIFICATION_ATTEMPTS - 1) {
                std::this_thread::sleep_for(POSITION_CLOSE_WAIT_TIME);
            }
        }
        
        TradingLogs::log_debug_position_still_exists(side_str);
        return false;
    } catch (const std::exception& e) {
        TradingLogs::log_market_status(false, "Position closure failed: " + std::string(e.what()));
        return false;
    }
}

bool OrderExecutionEngine::can_execute_new_position(OrderSide /*side*/, int current_qty) const {
    // Can always execute if flat
    if (is_flat_position(current_qty)) {
        return true;
    }
    
    // Can execute if multiple positions are allowed
    if (config.risk.allow_multiple_positions) {
        return true;
    }
    
    // Cannot execute if we have a position and multiple positions are not allowed
    return false;
}

// Order validation and preparation
bool OrderExecutionEngine::validate_order_parameters(const ProcessedData& data, const StrategyLogic::PositionSizing& sizing) const {
    if (data.curr.c <= 0.0) {
        TradingLogs::log_trade_validation_failed("Invalid price data");
        return false;
    }
    
    if (sizing.quantity <= 0) {
        TradingLogs::log_trade_validation_failed("Invalid quantity");
        return false;
    }
    
    if (sizing.risk_amount <= 0.0) {
        TradingLogs::log_trade_validation_failed("Invalid risk amount");
        return false;
    }
    
    return true;
}

StrategyLogic::ExitTargets OrderExecutionEngine::calculate_exit_targets(OrderSide side, const ProcessedData& data, const StrategyLogic::PositionSizing& sizing) const {
    return StrategyLogic::compute_exit_targets(
        to_side_string(side), 
        data.curr.c, 
        sizing.risk_amount, 
        config.strategy.rr_ratio, 
        config
    );
}

// Utility methods
std::string OrderExecutionEngine::to_side_string(OrderSide side) const {
    return (side == OrderSide::Buy) ? SIGNAL_BUY : SIGNAL_SELL;
}

std::string OrderExecutionEngine::to_opposite_side_string(OrderSide side) const {
    return (side == OrderSide::Buy) ? POSITION_SHORT : POSITION_LONG;
}

bool OrderExecutionEngine::is_long_position(int qty) const {
    return qty > 0;
}

bool OrderExecutionEngine::is_short_position(int qty) const {
    return qty < 0;
}

bool OrderExecutionEngine::is_flat_position(int qty) const {
    return qty == 0;
}

} // namespace Core
} // namespace AlpacaTrader
