#include "order_execution_engine.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/monitoring/system_monitor.hpp"
#include <thread>
#include <chrono>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

OrderExecutionEngine::OrderExecutionEngine(API::AlpacaClient& client_ref, AccountManager& account_mgr, const TraderConfig& cfg, DataSyncReferences& data_sync_ref)
    : client(client_ref), account_manager(account_mgr), config(cfg), data_sync(data_sync_ref) {}

void OrderExecutionEngine::execute_trade(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& sd) {
    TradingLogs::log_order_execution_header();
    
    try {
        if (!validate_order_parameters(data, sizing)) {
            TradingLogs::log_market_status(false, "Order validation failed - aborting trade execution");
            return;
        }
        
        if (data.curr.c <= 0.0) {
            TradingLogs::log_market_status(false, "Invalid price data - price is zero or negative");
            return;
        }
        
        if (sizing.quantity <= 0) {
            TradingLogs::log_market_status(false, "Invalid quantity - must be positive");
            return;
        }
        
        double buying_power = account_manager.fetch_buying_power();
        double required_capital = data.curr.c * sizing.quantity;
        double safety_margin = config.strategy.short_safety_margin > 0 ? config.strategy.short_safety_margin : 0.9;
        
        if (required_capital > buying_power * safety_margin) {
            TradingLogs::log_market_status(false, "Insufficient buying power - required: $" + 
                                std::to_string(required_capital) + ", available: $" + std::to_string(buying_power) + 
                                ", safety margin: " + std::to_string(safety_margin * 100) + "%");
            return;
        }
        
    } catch (const std::exception& e) {
        TradingLogs::log_market_status(false, "Error in trade validation: " + std::string(e.what()));
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
        // For SELL signals, determine the correct order side based on current position
        if (current_qty == 0) {
            // No position: open short position (use sell side for short)
    if (!client.check_short_availability(config.target.symbol, sizing.quantity)) {
        TradingLogs::log_market_status(false, "SELL signal blocked - insufficient short availability");
        AlpacaTrader::Core::Monitoring::SystemMonitor::instance().record_short_blocked(config.target.symbol);
        return;
    }
            TradingLogs::log_market_status(true, "SELL signal - opening short position with bracket order");
            execute_order(OrderSide::Sell, data, current_qty, sizing);
        } else if (current_qty > 0) {
            // Long position: close long position (sell to close)
            TradingLogs::log_market_status(true, "SELL signal - closing long position with market order");
            execute_order(OrderSide::Sell, data, current_qty, sizing);
        } else {
            // Short position: close short position (buy to close)
            TradingLogs::log_market_status(true, "SELL signal - closing short position with market order");
            execute_order(OrderSide::Buy, data, current_qty, sizing);
        }
    } else {
        TradingLogs::log_no_trading_pattern();
    }
}

// Core execution method - unified for both buy and sell orders
void OrderExecutionEngine::execute_order(OrderSide side, const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing) {
    TradingLogs::log_debug_position_data(current_qty, data.pos_details.current_value, data.pos_details.qty, is_long_position(current_qty), is_short_position(current_qty));
    
    // Check wash trade prevention first (if enabled)
    if (config.timing.enable_wash_trade_prevention) {
        if (!can_place_order_now()) {
            TradingLogs::log_market_status(false, "Order blocked - minimum order interval not met (wash trade prevention)");
            return;
        } else {
            TradingLogs::log_market_status(true, "Wash trade check passed - order allowed");
        }
    } else {
        TradingLogs::log_market_status(true, "Wash trade prevention disabled - order allowed");
    }
    
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
    
    // Determine if this is opening a new position or closing an existing one
    if (current_qty == 0) {
        // Opening new position: use bracket order
        StrategyLogic::ExitTargets targets = calculate_exit_targets(side, data, sizing);
        execute_bracket_order(side, data, sizing, targets);
    } else {
        // Closing existing position: use regular market order
        execute_market_order(side, data, sizing);
    }
    
    // Update the last order timestamp after successful order placement
    update_last_order_timestamp();
}

// Execute bracket order with proper validation
void OrderExecutionEngine::execute_bracket_order(OrderSide side, const ProcessedData& data, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::ExitTargets& targets) {
    std::string side_str = to_side_string(side);
    
    // Use consolidated logging instead of multiple separate tables
    TradingLogs::log_comprehensive_order_execution("Bracket Order", side_str, sizing.quantity, 
                                                  data.curr.c, data.atr, data.pos_details.qty, sizing.risk_amount,
                                                  targets.stop_loss, targets.take_profit, 
                                                  config.target.symbol, "execute_bracket_order");
    
    // Also log the calculated entry and exit values for bracket orders
    TradingLogs::log_exit_targets_table(side_str, data.curr.c, sizing.risk_amount, config.strategy.rr_ratio, targets.stop_loss, targets.take_profit);
    
    try {
        bool has_pending = false;
        try {
            has_pending = client.has_pending_orders(config.target.symbol);
        } catch (const std::exception& e) {
            TradingLogs::log_market_status(false, "Error checking pending orders: " + std::string(e.what()));
        }
        
        if (has_pending) {
            if (should_cancel_existing_orders(side_str)) {
                TradingLogs::log_market_status(false, "Found conflicting orders - cancelling before new bracket order");
                try {
                    client.cancel_pending_orders(config.target.symbol);
                    int cancel_wait_ms = config.strategy.short_retry_delay_ms > 0 ? 
                        config.strategy.short_retry_delay_ms / 5 : 200;
                    std::this_thread::sleep_for(std::chrono::milliseconds(cancel_wait_ms));
                } catch (const std::exception& e) {
                    TradingLogs::log_market_status(false, "Error cancelling orders: " + std::string(e.what()));
                }
            } else {
                TradingLogs::log_market_status(true, "Found non-conflicting orders - proceeding with new order");
            }
        }
        
        bool order_placed = false;
        int max_retries = config.strategy.max_retries;
        int retry_delay_ms = config.strategy.retry_delay_ms;
        
        for (int attempt = 1; attempt <= max_retries && !order_placed; ++attempt) {
            try {
                client.place_bracket_order(OrderRequest{side_str, sizing.quantity, targets.take_profit, targets.stop_loss});
                order_placed = true;
                TradingLogs::log_market_status(true, "Bracket order placed successfully on attempt " + std::to_string(attempt));
                AlpacaTrader::Core::Monitoring::SystemMonitor::instance().record_order_placed(true);
            } catch (const std::exception& e) {
                if (attempt < max_retries) {
                    TradingLogs::log_market_status(false, "Order attempt " + std::to_string(attempt) + " failed, retrying: " + std::string(e.what()));
                    int delay_ms = retry_delay_ms * attempt;
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                } else {
                    TradingLogs::log_market_status(false, "Order execution failed after " + std::to_string(max_retries) + " attempts: " + std::string(e.what()));
                    AlpacaTrader::Core::Monitoring::SystemMonitor::instance().record_order_placed(false, std::string(e.what()));
                }
            }
        }
    } catch (const std::exception& e) {
        TradingLogs::log_market_status(false, "Order execution failed: " + std::string(e.what()));
    }
}

// Execute regular market order for closing positions
void OrderExecutionEngine::execute_market_order(OrderSide side, const ProcessedData& data, const StrategyLogic::PositionSizing& sizing) {
    std::string side_str = to_side_string(side);
    
    // Use consolidated logging instead of multiple separate debug messages
    TradingLogs::log_comprehensive_order_execution("Market Order", side_str, sizing.quantity, 
                                                  data.curr.c, data.atr, data.pos_details.qty, sizing.risk_amount,
                                                  0.0, 0.0, // No stop loss or take profit for market orders
                                                  config.target.symbol, "execute_market_order");
    
    try {
        // Check for and cancel any pending orders before placing new ones
        if (client.has_pending_orders(config.target.symbol)) {
            TradingLogs::log_market_status(false, "Found pending orders - cancelling before new order");
            client.cancel_pending_orders(config.target.symbol);
            
            // Wait a moment for order cancellation to process
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // For closing positions, we need to determine the correct side
        // If we have a long position and want to close it, we need to sell
        // If we have a short position and want to close it, we need to buy
        client.submit_market_order(config.target.symbol, side_str, sizing.quantity);
        TradingLogs::log_market_status(true, "Market order submitted successfully");
    } catch (const std::exception& e) {
        TradingLogs::log_market_status(false, "Market order execution failed: " + std::string(e.what()));
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
    
    // Additional validation for order rejection prevention using config values
    if (sizing.quantity > config.strategy.max_quantity_per_trade) {
        TradingLogs::log_trade_validation_failed("Quantity too large - max " + std::to_string(config.strategy.max_quantity_per_trade) + " shares");
        return false;
    }
    
    // Validate price is within configured range
    if (data.curr.c < config.strategy.min_price_threshold || data.curr.c > config.strategy.max_price_threshold) {
        TradingLogs::log_trade_validation_failed("Price out of configured range: $" + std::to_string(config.strategy.min_price_threshold) + " - $" + std::to_string(config.strategy.max_price_threshold));
        return false;
    }
    
    // Check if order value exceeds configured maximum
    double order_value = data.curr.c * sizing.quantity;
    if (order_value > config.strategy.max_order_value) {
        TradingLogs::log_trade_validation_failed("Order value too large - max $" + std::to_string(config.strategy.max_order_value));
        return false;
    }
    
    return true;
}

StrategyLogic::ExitTargets OrderExecutionEngine::calculate_exit_targets(OrderSide side, const ProcessedData& data, const StrategyLogic::PositionSizing& sizing) const {
    double entry_price = data.curr.c;
    
    // Use real-time price if configured and available
    if (config.strategy.use_realtime_price_for_orders) {
        double realtime_price = client.get_current_price(config.target.symbol);
        if (realtime_price > 0.0) {
            entry_price = realtime_price;
            TradingLogs::log_realtime_price_used(realtime_price, data.curr.c);
        } else {
            TradingLogs::log_realtime_price_fallback(data.curr.c);
        }
    }
    
    return StrategyLogic::compute_exit_targets(
        to_side_string(side), 
        entry_price, 
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

// Order timing methods for wash trade prevention
bool OrderExecutionEngine::can_place_order_now() const {
    auto now = std::chrono::steady_clock::now();
    auto last_order = data_sync.last_order_timestamp->load();
    
    // If no previous order, allow
    if (last_order == std::chrono::steady_clock::time_point::min()) {
        TradingLogs::log_market_status(true, "No previous orders - wash trade check passed");
        return true;
    }
    
    // Check if enough time has passed
    auto time_since_last_order = std::chrono::duration_cast<std::chrono::seconds>(now - last_order);
    int min_interval = config.timing.min_order_interval_sec;
    int elapsed_seconds = time_since_last_order.count();
    
    if (elapsed_seconds >= min_interval) {
        TradingLogs::log_market_status(true, "Wash trade check passed - " + std::to_string(elapsed_seconds) + "s elapsed (required: " + std::to_string(min_interval) + "s)");
        return true;
    } else {
        int remaining_seconds = min_interval - elapsed_seconds;
        TradingLogs::log_market_status(false, "Wash trade prevention active - " + std::to_string(elapsed_seconds) + "s elapsed, " + std::to_string(remaining_seconds) + "s remaining");
        return false;
    }
}

void OrderExecutionEngine::update_last_order_timestamp() {
    data_sync.last_order_timestamp->store(std::chrono::steady_clock::now());
}

bool OrderExecutionEngine::is_flat_position(int qty) const {
    return qty == 0;
}

bool OrderExecutionEngine::should_cancel_existing_orders(const std::string& new_side) const {
    return true;
}

} // namespace Core
} // namespace AlpacaTrader
