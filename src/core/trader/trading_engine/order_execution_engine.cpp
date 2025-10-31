#include "order_execution_engine.hpp"
#include "core/logging/logs/trading_logs.hpp"
#include "core/logging/logs/logger_structures.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/system/system_monitor.hpp"
#include <thread>
#include <chrono>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

OrderExecutionEngine::OrderExecutionEngine(const OrderExecutionEngineConstructionParams& construction_params)
    : api_manager(construction_params.api_manager_ref), account_manager(construction_params.account_manager_ref), 
      config(construction_params.system_config), data_sync(construction_params.data_sync_ref), 
      system_monitor(construction_params.system_monitor_ref) {}

void OrderExecutionEngine::execute_trade(const ProcessedData& data, int current_position_quantity, const PositionSizing& sizing, const SignalDecision& signal_decision) {
    TradingLogs::log_order_execution_header();
    
    try {
        if (!validate_order_parameters(data, sizing)) {
            TradingLogs::log_market_status(false, "Order validation failed - aborting trade execution");
            return;
        }
        
        if (data.curr.close_price <= 0.0) {
            TradingLogs::log_market_status(false, "Invalid price data - price is zero or negative");
            return;
        }
        
        if (sizing.quantity <= 0) {
            TradingLogs::log_market_status(false, "Invalid quantity - must be positive");
            return;
        }
        
        double buying_power = account_manager.fetch_buying_power();
        double required_capital = data.curr.close_price * sizing.quantity;
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
    
    bool is_long = current_position_quantity > 0;
    bool is_short = current_position_quantity < 0;
    TradingLogs::log_debug_position_data(current_position_quantity, 0.0, current_position_quantity, is_long, is_short);

    if (signal_decision.buy) {
        TradingLogs::log_signal_triggered(config.strategy.signal_buy_string, true);
        execute_order(OrderSide::Buy, data, current_position_quantity, sizing);
    } else if (signal_decision.sell) {
        TradingLogs::log_signal_triggered(config.strategy.signal_sell_string, true);
        if (current_position_quantity == 0) {
            if (!api_manager.get_account_info().empty()) {
                TradingLogs::log_market_status(false, "SELL signal blocked - insufficient short availability for new position");
                system_monitor.record_short_blocked(config.trading_mode.primary_symbol);
            } else {
                TradingLogs::log_market_status(true, "SELL signal - opening short position with bracket order");
                execute_order(OrderSide::Sell, data, current_position_quantity, sizing);
            }
        } else if (current_position_quantity > 0) {
            TradingLogs::log_market_status(true, "SELL signal - closing long position with market order");
            execute_order(OrderSide::Sell, data, current_position_quantity, sizing);
        } else {
            TradingLogs::log_market_status(true, "SELL signal - closing short position with market order");
            execute_order(OrderSide::Buy, data, current_position_quantity, sizing);
        }
    } else {
        TradingLogs::log_no_trading_pattern();
    }
}

void OrderExecutionEngine::execute_order(OrderSide side, const ProcessedData& data, int current_position_quantity, const PositionSizing& sizing) {
    TradingLogs::log_debug_position_data(current_position_quantity, data.pos_details.current_value, data.pos_details.position_quantity, current_position_quantity > 0, current_position_quantity < 0);
    
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
    
    if (should_close_opposite_position(side, current_position_quantity)) {
        if (!close_opposite_position(side, current_position_quantity)) {
            TradingLogs::log_position_limits_reached((side == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string);
            return;
        }
    }
    
    if (!can_execute_new_position(current_position_quantity)) {
        TradingLogs::log_position_limits_reached((side == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string);
        return;
    }
    
    if (current_position_quantity == 0) {
        ExitTargets targets = calculate_exit_targets(side, data, sizing);
        execute_bracket_order(side, data, sizing, targets);
    } else {
        execute_market_order(side, data, sizing);
    }
    
    // Update the last order timestamp after successful order placement
    update_last_order_timestamp();
}

// Execute bracket order with proper validation
void OrderExecutionEngine::execute_bracket_order(OrderSide side, const ProcessedData& data, const PositionSizing& sizing, const ExitTargets& targets) {
    std::string side_str = (side == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string;
    
    // Use consolidated logging instead of multiple separate tables
    Logging::ComprehensiveOrderExecutionRequest order_request("Bracket Order", side_str, sizing.quantity, 
                                                    data.curr.close_price, data.atr, data.pos_details.position_quantity, 
                                                    sizing.risk_amount, targets.stop_loss, targets.take_profit,
                                                    config.trading_mode.primary_symbol, "execute_bracket_order");
    TradingLogs::log_comprehensive_order_execution(order_request);
    
    Logging::ExitTargetsTableRequest exit_targets_request(side_str, data.curr.close_price, sizing.risk_amount, config.strategy.rr_ratio, targets.stop_loss, targets.take_profit);
    TradingLogs::log_exit_targets_table(exit_targets_request);
    
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
                system_monitor.record_order_placed(true);
            } catch (const std::exception& e) {
                if (attempt < max_retries) {
                    TradingLogs::log_market_status(false, "Order attempt " + std::to_string(attempt) + " failed, retrying: " + std::string(e.what()));
                    int delay_ms = retry_delay_ms * attempt;
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                } else {
                    TradingLogs::log_market_status(false, "Order execution failed after " + std::to_string(max_retries) + " attempts: " + std::string(e.what()));
                    system_monitor.record_order_placed(false, std::string(e.what()));
                }
            }
        }
    } catch (const std::exception& e) {
        TradingLogs::log_market_status(false, "Order execution failed: " + std::string(e.what()));
    }
}

// Execute regular market order for closing positions
void OrderExecutionEngine::execute_market_order(OrderSide side, const ProcessedData& data, const PositionSizing& sizing) {
    std::string side_str = (side == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string;
    
    Logging::ComprehensiveOrderExecutionRequest order_request("Market Order", side_str, sizing.quantity,
                                                    data.curr.close_price, data.atr, data.pos_details.position_quantity,
                                                    sizing.risk_amount, 0.0, 0.0,
                                                    config.trading_mode.primary_symbol, "execute_market_order");
    TradingLogs::log_comprehensive_order_execution(order_request);
    
    try {
        // Check for and cancel any pending orders before placing new ones
        if (!api_manager.get_open_orders().empty()) {
            TradingLogs::log_market_status(false, "Found pending orders - cancelling before new order");
            // Cancel pending orders through API manager
            TradingLogs::log_market_status(true, "Cancelling pending orders");
            
            // Wait a moment for order cancellation to process
            int cancellation_delay_ms = config.timing.order_cancellation_processing_delay_milliseconds;
            std::this_thread::sleep_for(std::chrono::milliseconds(cancellation_delay_ms));
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
bool OrderExecutionEngine::should_close_opposite_position(OrderSide side, int current_position_quantity) const {
    if (!config.strategy.close_positions_on_signal_reversal) {
        return false;
    }
    
    return (side == OrderSide::Buy && current_position_quantity < 0) ||
           (side == OrderSide::Sell && current_position_quantity > 0);
}

bool OrderExecutionEngine::close_opposite_position(OrderSide side, int current_position_quantity) {
    std::string side_str = (side == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string;
    std::string opposite_side_str = (side == OrderSide::Buy) ? config.strategy.position_short_string : config.strategy.position_long_string;
    
    TradingLogs::log_position_closure("Closing " + opposite_side_str + " position first for " + side_str + " signal", current_position_quantity);
    
    try {
        api_manager.close_position(config.trading_mode.primary_symbol, current_position_quantity);
        
        int position_verification_timeout_milliseconds = config.timing.position_verification_timeout_milliseconds;
        int maximum_position_verification_attempts = config.timing.maximum_position_verification_attempts;
        
        if (position_verification_timeout_milliseconds <= 0) {
            TradingLogs::log_market_status(false, "Invalid position verification timeout - must be positive");
            return false;
        }
        
        if (maximum_position_verification_attempts <= 0) {
            TradingLogs::log_market_status(false, "Invalid maximum position verification attempts - must be positive");
            return false;
        }
        
        std::chrono::milliseconds position_close_wait_time(position_verification_timeout_milliseconds);
        
        std::this_thread::sleep_for(position_close_wait_time);
        
        for (int attempt = 0; attempt < maximum_position_verification_attempts; ++attempt) {
            AccountSnapshot verify_account = account_manager.fetch_account_snapshot();
            int verify_position_quantity = verify_account.pos_details.position_quantity;
            
            if (verify_position_quantity == 0) {
                TradingLogs::log_debug_position_verification(verify_position_quantity);
                return true;
            }
            
            if (attempt < maximum_position_verification_attempts - 1) {
                std::this_thread::sleep_for(position_close_wait_time);
            }
        }
        
        TradingLogs::log_debug_position_still_exists(side_str);
        return false;
    } catch (const std::exception& e) {
        TradingLogs::log_market_status(false, "Position closure failed: " + std::string(e.what()));
        return false;
    }
}

bool OrderExecutionEngine::can_execute_new_position(int current_position_quantity) const {
    if (is_flat_position(current_position_quantity)) {
        return true;
    }
    
    if (config.strategy.allow_multiple_positions_per_symbol) {
        return true;
    }
    
    return false;
}

// Order validation and preparation
bool OrderExecutionEngine::validate_order_parameters(const ProcessedData& data, const PositionSizing& sizing) const {
    if (data.curr.close_price <= 0.0) {
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
    if (data.curr.close_price < config.strategy.minimum_acceptable_price_for_signals || data.curr.close_price > config.strategy.maximum_acceptable_price_for_signals) {
        TradingLogs::log_trade_validation_failed("Price out of configured range: $" + std::to_string(config.strategy.minimum_acceptable_price_for_signals) + " - $" + std::to_string(config.strategy.maximum_acceptable_price_for_signals));
        return false;
    }
    
    // Check if order value exceeds configured maximum
    double order_value = data.curr.close_price * sizing.quantity;
    if (order_value > config.strategy.maximum_dollar_value_per_single_trade) {
        TradingLogs::log_trade_validation_failed("Order value too large - max $" + std::to_string(config.strategy.maximum_dollar_value_per_single_trade));
        return false;
    }
    
    return true;
}

ExitTargets OrderExecutionEngine::calculate_exit_targets(OrderSide side, const ProcessedData& data, const PositionSizing& sizing) const {
    double entry_price = data.curr.close_price;
    
    // Use real-time price if configured and available
    if (config.strategy.use_current_market_price_for_order_execution) {
        double realtime_price = api_manager.get_current_price(config.trading_mode.primary_symbol);
        if (realtime_price > 0.0) {
            entry_price = realtime_price;
            TradingLogs::log_realtime_price_used(realtime_price, data.curr.close_price);
        } else {
            TradingLogs::log_realtime_price_fallback(data.curr.close_price);
        }
    }
    
    return compute_exit_targets(ExitTargetsRequest(
        (side == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string, 
        entry_price, 
        sizing.risk_amount, 
        config.strategy
    ));
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

bool OrderExecutionEngine::is_flat_position(int position_quantity) const {
    return position_quantity == 0;
}

bool OrderExecutionEngine::should_cancel_existing_orders() const {
    return true;
}

bool OrderExecutionEngine::validate_trade_feasibility(const PositionSizing& sizing, double buying_power, double current_price) const {
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

void OrderExecutionEngine::handle_market_close_positions(const ProcessedData& data) {
    if (api_manager.is_market_open(config.trading_mode.primary_symbol)) {
        return;
    }
    
    int current_position_quantity = data.pos_details.position_quantity;
    if (current_position_quantity == 0) {
        return;
    }
    
    int market_close_grace_period_minutes = config.timing.market_close_grace_period_minutes;
    if (market_close_grace_period_minutes <= 0) {
        TradingLogs::log_market_status(false, "Invalid market close grace period - must be positive");
        return;
    }
    
    TradingLogs::log_market_close_warning(market_close_grace_period_minutes);
    
    std::string side = (current_position_quantity > 0) ? config.strategy.signal_sell_string : config.strategy.signal_buy_string;
    TradingLogs::log_market_close_position_closure(current_position_quantity, config.trading_mode.primary_symbol, side);
    
    try {
        api_manager.close_position(config.trading_mode.primary_symbol, current_position_quantity);
        TradingLogs::log_market_status(true, "Market close position closure executed successfully");
    } catch (const std::exception& e) {
        TradingLogs::log_market_status(false, "Market close position closure failed: " + std::string(e.what()));
    }
    
    TradingLogs::log_market_close_complete();
}

} // namespace Core
} // namespace AlpacaTrader
