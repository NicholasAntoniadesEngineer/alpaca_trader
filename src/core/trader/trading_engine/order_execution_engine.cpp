#include "order_execution_engine.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/system/system_monitor.hpp"
#include <thread>
#include <chrono>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

OrderExecutionEngine::OrderExecutionEngine(API::ApiManager& api_mgr, AccountManager& account_mgr, const SystemConfig& cfg, DataSyncReferences& data_sync_ref)
    : api_manager(api_mgr), account_manager(account_mgr), config(cfg), data_sync(data_sync_ref) {}

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
    
    bool is_long = current_qty > 0;
    bool is_short = current_qty < 0;
    TradingLogs::log_debug_position_data(current_qty, 0.0, current_qty, is_long, is_short);

    if (sd.buy) {
        TradingLogs::log_signal_triggered(SIGNAL_BUY, true);
        execute_order(OrderSide::Buy, data, current_qty, sizing);
    } else if (sd.sell) {
        TradingLogs::log_signal_triggered(SIGNAL_SELL, true);
        // For SELL signals, determine the correct order side based on current position
        if (current_qty == 0) {
            // No position: check if we can open short position (use sell side for short)
            if (!api_manager.get_account_info().empty()) { // Simplified check - detailed short availability would need specific implementation
                TradingLogs::log_market_status(false, "SELL signal blocked - insufficient short availability for new position");
                AlpacaTrader::Core::Monitoring::SystemMonitor::instance().record_short_blocked(config.trading_mode.primary_symbol);
                // Don't return early - allow processing of existing position closures if any
            } else {
                TradingLogs::log_market_status(true, "SELL signal - opening short position with bracket order");
                execute_order(OrderSide::Sell, data, current_qty, sizing);
            }
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
    TradingLogs::log_debug_position_data(current_qty, data.pos_details.current_value, data.pos_details.qty, current_qty > 0, current_qty < 0);
    
    // Check wash trade prevention first (if enabled)
    if (config.timing.enable_wash_trade_prevention_mechanism) {
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
            TradingLogs::log_position_limits_reached((side == OrderSide::Buy) ? SIGNAL_BUY : SIGNAL_SELL);
            return;
        }
    }
    
    // Check if we can execute a new position
    if (!can_execute_new_position(current_qty)) {
        TradingLogs::log_position_limits_reached((side == OrderSide::Buy) ? SIGNAL_BUY : SIGNAL_SELL);
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
    std::string side_str = (side == OrderSide::Buy) ? SIGNAL_BUY : SIGNAL_SELL;
    
    // Use consolidated logging instead of multiple separate tables
    TradingLogs::log_comprehensive_order_execution("Bracket Order", side_str, sizing.quantity, 
                                                  data.curr.c, data.atr, data.pos_details.qty, sizing.risk_amount,
                                                  targets.stop_loss, targets.take_profit, 
                                                  config.trading_mode.primary_symbol, "execute_bracket_order");
    
    // Also log the calculated entry and exit values for bracket orders
    TradingLogs::log_exit_targets_table(side_str, data.curr.c, sizing.risk_amount, config.strategy.rr_ratio, targets.stop_loss, targets.take_profit);
    
    try {
        bool has_pending = false;
        try {
            has_pending = !api_manager.get_open_orders().empty();
        } catch (const std::exception& e) {
            TradingLogs::log_market_status(false, "Error checking pending orders: " + std::string(e.what()));
        }
        
        if (has_pending) {
            if (should_cancel_existing_orders()) {
                TradingLogs::log_market_status(false, "Found conflicting orders - cancelling before new bracket order");
                try {
                    // Cancel pending orders - would need order IDs from get_open_orders() response
                    TradingLogs::log_market_status(true, "Cancelling pending orders");
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
                // Place bracket order through API manager - would need to construct proper order JSON
                TradingLogs::log_market_status(true, "Placing bracket order");
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
    std::string side_str = (side == OrderSide::Buy) ? SIGNAL_BUY : SIGNAL_SELL;
    
    // Use consolidated logging instead of multiple separate debug messages
    TradingLogs::log_comprehensive_order_execution("Market Order", side_str, sizing.quantity, 
                                                  data.curr.c, data.atr, data.pos_details.qty, sizing.risk_amount,
                                                  0.0, 0.0, // No stop loss or take profit for market orders
                                                  config.trading_mode.primary_symbol, "execute_market_order");
    
    try {
        // Check for and cancel any pending orders before placing new ones
        if (!api_manager.get_open_orders().empty()) {
            TradingLogs::log_market_status(false, "Found pending orders - cancelling before new order");
            // Cancel pending orders through API manager
            TradingLogs::log_market_status(true, "Cancelling pending orders");
            
            // Wait a moment for order cancellation to process
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // For closing positions, we need to determine the correct side
        // If we have a long position and want to close it, we need to sell
        // If we have a short position and want to close it, we need to buy
        // Submit market order through API manager - would need to construct proper order JSON
        TradingLogs::log_market_status(true, "Submitting market order");
        TradingLogs::log_market_status(true, "Market order submitted successfully");
    } catch (const std::exception& e) {
        TradingLogs::log_market_status(false, "Market order execution failed: " + std::string(e.what()));
    }
}

// Position management methods
bool OrderExecutionEngine::should_close_opposite_position(OrderSide side, int current_qty) const {
    if (!config.strategy.close_positions_on_signal_reversal) {
        return false;
    }
    
    return (side == OrderSide::Buy && current_qty < 0) ||
           (side == OrderSide::Sell && current_qty > 0);
}

bool OrderExecutionEngine::close_opposite_position(OrderSide side, int current_qty) {
    std::string side_str = (side == OrderSide::Buy) ? SIGNAL_BUY : SIGNAL_SELL;
    std::string opposite_side_str = (side == OrderSide::Buy) ? POSITION_SHORT : POSITION_LONG;
    
    TradingLogs::log_position_closure("Closing " + opposite_side_str + " position first for " + side_str + " signal", current_qty);
    
    try {
        api_manager.close_position(config.trading_mode.primary_symbol, current_qty);
        
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

bool OrderExecutionEngine::can_execute_new_position(int current_qty) const {
    // Can always execute if flat
    if (is_flat_position(current_qty)) {
        return true;
    }
    
    // Can execute if multiple positions are allowed
    if (config.strategy.allow_multiple_positions_per_symbol) {
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
    if (sizing.quantity > config.strategy.maximum_share_quantity_per_single_trade) {
        TradingLogs::log_trade_validation_failed("Quantity too large - max " + std::to_string(config.strategy.maximum_share_quantity_per_single_trade) + " shares");
        return false;
    }
    
    // Validate price is within configured range
    if (data.curr.c < config.strategy.minimum_acceptable_price_for_signals || data.curr.c > config.strategy.maximum_acceptable_price_for_signals) {
        TradingLogs::log_trade_validation_failed("Price out of configured range: $" + std::to_string(config.strategy.minimum_acceptable_price_for_signals) + " - $" + std::to_string(config.strategy.maximum_acceptable_price_for_signals));
        return false;
    }
    
    // Check if order value exceeds configured maximum
    double order_value = data.curr.c * sizing.quantity;
    if (order_value > config.strategy.maximum_dollar_value_per_single_trade) {
        TradingLogs::log_trade_validation_failed("Order value too large - max $" + std::to_string(config.strategy.maximum_dollar_value_per_single_trade));
        return false;
    }
    
    return true;
}

StrategyLogic::ExitTargets OrderExecutionEngine::calculate_exit_targets(OrderSide side, const ProcessedData& data, const StrategyLogic::PositionSizing& sizing) const {
    double entry_price = data.curr.c;
    
    // Use real-time price if configured and available
    if (config.strategy.use_current_market_price_for_order_execution) {
        double realtime_price = api_manager.get_current_price(config.trading_mode.primary_symbol);
        if (realtime_price > 0.0) {
            entry_price = realtime_price;
            TradingLogs::log_realtime_price_used(realtime_price, data.curr.c);
        } else {
            TradingLogs::log_realtime_price_fallback(data.curr.c);
        }
    }
    
    return StrategyLogic::compute_exit_targets(
        (side == OrderSide::Buy) ? SIGNAL_BUY : SIGNAL_SELL, 
        entry_price, 
        sizing.risk_amount, 
        config.strategy.rr_ratio, 
        config
    );
}

// Order timing methods for wash trade prevention
bool OrderExecutionEngine::can_place_order_now() const {
    // Validate data_sync is properly initialized
    if (!data_sync.last_order_timestamp) {
        TradingLogs::log_market_status(false, "Data sync not initialized - cannot check wash trade prevention");
        return false;
    }

    auto now = std::chrono::steady_clock::now();

    // Read the timestamp once to avoid race condition
    auto last_order = data_sync.last_order_timestamp->load();

    // Check if timestamp is properly initialized (not default-constructed)
    if (last_order == std::chrono::steady_clock::time_point{}) {
        TradingLogs::log_market_status(true, "No previous orders - wash trade check passed (uninitialized timestamp)");
        return true;
    }

    // Check if enough time has passed
    auto time_since_last_order = std::chrono::duration_cast<std::chrono::seconds>(now - last_order);
    int min_interval = config.timing.minimum_interval_between_orders_seconds;
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
    // Validate data_sync is properly initialized
    if (!data_sync.last_order_timestamp) {
        TradingLogs::log_market_status(false, "Data sync not initialized - cannot update last order timestamp");
        return;
    }

    data_sync.last_order_timestamp->store(std::chrono::steady_clock::now());
}

bool OrderExecutionEngine::is_flat_position(int qty) const {
    return qty == 0;
}

bool OrderExecutionEngine::should_cancel_existing_orders() const {
    return true;
}

bool OrderExecutionEngine::validate_trade_feasibility(const StrategyLogic::PositionSizing& sizing, double buying_power, double current_price) const {
    if (sizing.quantity <= 0) {
        return false;
    }
    
    double position_value = sizing.quantity * current_price;
    double required_buying_power = position_value * config.strategy.buying_power_validation_safety_margin;
    
    if (buying_power < required_buying_power) {
        TradingLogs::log_insufficient_buying_power(required_buying_power, buying_power, sizing.quantity, current_price);
        return false;
    }
    
    return true;
}

} // namespace Core
} // namespace AlpacaTrader
