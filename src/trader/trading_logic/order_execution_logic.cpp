#include "order_execution_logic.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "json/json.hpp"
#include <thread>
#include <chrono>
#include <stdexcept>
#include <cmath>

using json = nlohmann::json;

namespace AlpacaTrader {
namespace Core {

OrderExecutionLogic::OrderExecutionLogic(const OrderExecutionLogicConstructionParams& construction_params)
    : api_manager(construction_params.api_manager_ref), account_manager(construction_params.account_manager_ref), 
      config(construction_params.system_config), data_sync_ptr(construction_params.data_sync_ptr) {}

void OrderExecutionLogic::execute_trade(const ProcessedData& processed_data_input, int current_position_quantity, const PositionSizing& position_sizing_input, const SignalDecision& signal_decision_input) {
    if (!validate_order_parameters(processed_data_input, position_sizing_input)) {
        throw std::runtime_error("Order validation failed - aborting trade execution");
    }
    
    if (processed_data_input.curr.close_price <= 0.0) {
        throw std::runtime_error("Invalid price data - price is zero or negative");
    }
    
    if (position_sizing_input.quantity <= 0) {
        throw std::runtime_error("Invalid quantity - must be positive");
    }
    
    double buying_power_amount = account_manager.fetch_buying_power();
    double required_capital_amount = processed_data_input.curr.close_price * position_sizing_input.quantity;
    if (config.strategy.short_safety_margin <= 0.0 || config.strategy.short_safety_margin > 1.0) {
        throw std::runtime_error("Invalid short_safety_margin - must be between 0.0 and 1.0, got: " + std::to_string(config.strategy.short_safety_margin));
    }
    double safety_margin = config.strategy.short_safety_margin;
    
    if (required_capital_amount > buying_power_amount * safety_margin) {
        throw std::runtime_error("Insufficient buying power - required: $" + 
                                std::to_string(required_capital_amount) + ", available: $" + std::to_string(buying_power_amount) + 
                                ", safety margin: " + std::to_string(safety_margin * config.strategy.percentage_calculation_multiplier) + "%");
    }

    if (signal_decision_input.buy) {
        execute_order(OrderSide::Buy, processed_data_input, current_position_quantity, position_sizing_input);
    } else if (signal_decision_input.sell) {
        if (current_position_quantity == 0) {
            if (!api_manager.get_account_info().empty()) {
                throw std::runtime_error("SELL signal blocked - insufficient short availability for new position");
            } else {
                execute_order(OrderSide::Sell, processed_data_input, current_position_quantity, position_sizing_input);
            }
        } else if (current_position_quantity > 0) {
            execute_order(OrderSide::Sell, processed_data_input, current_position_quantity, position_sizing_input);
        } else {
            execute_order(OrderSide::Buy, processed_data_input, current_position_quantity, position_sizing_input);
        }
    }
}

void OrderExecutionLogic::execute_order(OrderSide order_side_input, const ProcessedData& processed_data_input, int current_position_quantity, const PositionSizing& position_sizing_input) {
    // Check wash trade prevention first (if enabled)
    if (config.timing.enable_wash_trade_prevention_mechanism) {
        if (!can_place_order_now()) {
            throw std::runtime_error("Order blocked - minimum order interval not met (wash trade prevention)");
        }
    }
    
    if (should_close_opposite_position(order_side_input, current_position_quantity)) {
        if (!close_opposite_position(order_side_input, current_position_quantity)) {
            throw std::runtime_error("Position limits reached - could not close opposite position for " + 
                                   ((order_side_input == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string));
        }
    }
    
    if (!can_execute_new_position(current_position_quantity)) {
        throw std::runtime_error("Position limits reached - cannot execute new position for " + 
                               ((order_side_input == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string));
    }
    
    if (current_position_quantity == 0) {
        ExitTargets exit_targets_result = calculate_exit_targets(order_side_input, processed_data_input, position_sizing_input);
        execute_bracket_order(order_side_input, processed_data_input, position_sizing_input, exit_targets_result);
    } else {
        execute_market_order(order_side_input, processed_data_input, position_sizing_input);
    }
    
    // Update the last order timestamp after successful order placement
    update_last_order_timestamp();
}

// Execute bracket order with proper validation
void OrderExecutionLogic::execute_bracket_order(OrderSide order_side_input, const ProcessedData& processed_data_input, const PositionSizing& position_sizing_input, const ExitTargets& exit_targets_input) {
    bool has_pending_orders = false;
    try {
        has_pending_orders = !api_manager.get_open_orders().empty();
    } catch (const std::exception& exception_error) {
        throw std::runtime_error("Error checking pending orders: " + std::string(exception_error.what()));
    }
    
    if (has_pending_orders) {
        if (should_cancel_existing_orders()) {
            if (config.strategy.short_retry_delay_ms <= 0) {
                throw std::runtime_error("Invalid short_retry_delay_ms - must be greater than 0");
            }
            int cancel_wait_milliseconds = config.strategy.short_retry_delay_ms / 5;
            std::this_thread::sleep_for(std::chrono::milliseconds(cancel_wait_milliseconds));
        }
    }
    
    std::string order_side_string = (order_side_input == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string;
    std::string symbol_string = config.trading_mode.primary_symbol;
    int quantity_value = position_sizing_input.quantity;
    double entry_price_amount = processed_data_input.curr.close_price;
    
    if (symbol_string.empty()) {
        throw std::runtime_error("Symbol is required for bracket order");
    }
    
    if (quantity_value <= 0) {
        throw std::runtime_error("Quantity must be positive for bracket order");
    }
    
    if (entry_price_amount <= 0.0 || !std::isfinite(entry_price_amount)) {
        throw std::runtime_error("Invalid entry price for bracket order");
    }
    
    if (exit_targets_input.stop_loss <= 0.0 || !std::isfinite(exit_targets_input.stop_loss)) {
        throw std::runtime_error("Invalid stop loss for bracket order");
    }
    
    if (exit_targets_input.take_profit <= 0.0 || !std::isfinite(exit_targets_input.take_profit)) {
        throw std::runtime_error("Invalid take profit for bracket order");
    }
    
    bool order_placed_status = false;
    int max_retry_attempts = config.strategy.max_retries;
    int retry_delay_milliseconds = config.strategy.retry_delay_ms;
    
    for (int retry_attempt_number = 1; retry_attempt_number <= max_retry_attempts && !order_placed_status; ++retry_attempt_number) {
        try {
            json bracket_order_json;
            bracket_order_json["symbol"] = symbol_string;
            bracket_order_json["qty"] = std::to_string(quantity_value);
            bracket_order_json["side"] = order_side_string;
            bracket_order_json["type"] = "market";
            bracket_order_json["time_in_force"] = "day";
            bracket_order_json["order_class"] = "bracket";
            
            double stop_loss_price = exit_targets_input.stop_loss;
            double take_profit_price = exit_targets_input.take_profit;
            
            bracket_order_json["stop_loss"] = json::object();
            bracket_order_json["stop_loss"]["stop_price"] = std::to_string(stop_loss_price);
            bracket_order_json["stop_loss"]["limit_price"] = std::to_string(stop_loss_price);
            
            bracket_order_json["take_profit"] = json::object();
            bracket_order_json["take_profit"]["limit_price"] = std::to_string(take_profit_price);
            
            std::string order_json_string = bracket_order_json.dump();
            api_manager.place_order(order_json_string);
            order_placed_status = true;
        } catch (const std::exception& exception_error) {
            if (retry_attempt_number < max_retry_attempts) {
                int delay_milliseconds = retry_delay_milliseconds * retry_attempt_number;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_milliseconds));
            } else {
                throw std::runtime_error("Order execution failed after " + std::to_string(max_retry_attempts) + " attempts: " + std::string(exception_error.what()));
            }
        }
    }
    
    if (!order_placed_status) {
        throw std::runtime_error("Bracket order placement failed after all retry attempts");
    }
}

// Execute regular market order for closing positions
void OrderExecutionLogic::execute_market_order(OrderSide order_side_input, const ProcessedData& processed_data_input, const PositionSizing& position_sizing_input) {
    // Check for and cancel any pending orders before placing new ones
    if (!api_manager.get_open_orders().empty()) {
        // Wait a moment for order cancellation to process
        int cancellation_delay_ms = config.timing.order_cancellation_processing_delay_milliseconds;
        std::this_thread::sleep_for(std::chrono::milliseconds(cancellation_delay_ms));
    }
    
    std::string order_side_string = (order_side_input == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string;
    std::string symbol_string = config.trading_mode.primary_symbol;
    int quantity_value = position_sizing_input.quantity;
    double current_price_amount = processed_data_input.curr.close_price;
    
    if (symbol_string.empty()) {
        throw std::runtime_error("Symbol is required for market order");
    }
    
    if (quantity_value <= 0) {
        throw std::runtime_error("Quantity must be positive for market order");
    }
    
    if (current_price_amount <= 0.0 || !std::isfinite(current_price_amount)) {
        throw std::runtime_error("Invalid current price for market order");
    }
    
    json market_order_json;
    market_order_json["symbol"] = symbol_string;
    market_order_json["qty"] = std::to_string(quantity_value);
    market_order_json["side"] = order_side_string;
    market_order_json["type"] = "market";
    market_order_json["time_in_force"] = "day";
    
    std::string order_json_string = market_order_json.dump();
    api_manager.place_order(order_json_string);
}

// Position management methods
bool OrderExecutionLogic::should_close_opposite_position(OrderSide order_side_input, int current_position_quantity) const {
    if (!config.strategy.close_positions_on_signal_reversal) {
        return false;
    }
    
    return (order_side_input == OrderSide::Buy && current_position_quantity < 0) ||
           (order_side_input == OrderSide::Sell && current_position_quantity > 0);
}

bool OrderExecutionLogic::close_opposite_position(OrderSide order_side_input, int current_position_quantity) {
    // Validate order_side_input matches position direction
    bool position_is_long = current_position_quantity > 0;
    bool position_is_short = current_position_quantity < 0;
    bool order_is_buy = (order_side_input == OrderSide::Buy);
    bool order_is_sell = (order_side_input == OrderSide::Sell);
    
    if ((position_is_long && !order_is_sell) || (position_is_short && !order_is_buy)) {
        throw std::runtime_error("Order side does not match position direction for closure");
    }
    
    try {
        api_manager.close_position(config.trading_mode.primary_symbol, current_position_quantity);
        
        int position_verification_timeout_milliseconds = config.timing.position_verification_timeout_milliseconds;
        int maximum_position_verification_attempts = config.timing.maximum_position_verification_attempts;
        
        if (position_verification_timeout_milliseconds <= 0) {
            throw std::runtime_error("Invalid position verification timeout - must be positive");
        }
        
        if (maximum_position_verification_attempts <= 0) {
            throw std::runtime_error("Invalid maximum position verification attempts - must be positive");
        }
        
        std::chrono::milliseconds position_close_wait_time(position_verification_timeout_milliseconds);
        
        std::this_thread::sleep_for(position_close_wait_time);
        
        for (int verification_attempt_number = 0; verification_attempt_number < maximum_position_verification_attempts; ++verification_attempt_number) {
            AccountSnapshot verify_account_snapshot = account_manager.fetch_account_snapshot();
            int verify_position_quantity_result = verify_account_snapshot.pos_details.position_quantity;
            
            if (verify_position_quantity_result == 0) {
                return true;
            }
            
            if (verification_attempt_number < maximum_position_verification_attempts - 1) {
                std::this_thread::sleep_for(position_close_wait_time);
            }
        }
        
        return false;
    } catch (const std::exception& exception_error) {
        throw std::runtime_error("Position closure failed: " + std::string(exception_error.what()));
    }
}

bool OrderExecutionLogic::can_execute_new_position(int current_position_quantity) const {
    if (is_flat_position(current_position_quantity)) {
        return true;
    }
    
    if (config.strategy.allow_multiple_positions_per_symbol) {
        return true;
    }
    
    return false;
}

// Order validation and preparation
bool OrderExecutionLogic::validate_order_parameters(const ProcessedData& processed_data_input, const PositionSizing& position_sizing_input) const {
    if (processed_data_input.curr.close_price <= 0.0) {
        return false;
    }
    
    if (position_sizing_input.quantity <= 0) {
        return false;
    }
    
    if (position_sizing_input.risk_amount <= 0.0) {
        return false;
    }
    
    // Additional validation for order rejection prevention using config values
    if (position_sizing_input.quantity > config.strategy.maximum_share_quantity_per_single_trade) {
        return false;
    }
    
    // Validate price is within configured range
    if (processed_data_input.curr.close_price < config.strategy.minimum_acceptable_price_for_signals || processed_data_input.curr.close_price > config.strategy.maximum_acceptable_price_for_signals) {
        return false;
    }
    
    // Check if order value exceeds configured maximum
    double order_value_amount = processed_data_input.curr.close_price * position_sizing_input.quantity;
    if (order_value_amount > config.strategy.maximum_dollar_value_per_single_trade) {
        return false;
    }
    
    return true;
}

ExitTargets OrderExecutionLogic::calculate_exit_targets(OrderSide order_side_input, const ProcessedData& processed_data_input, const PositionSizing& position_sizing_input) const {
    double entry_price_amount = processed_data_input.curr.close_price;
    
    // Use real-time price if configured and available
    if (config.strategy.use_current_market_price_for_order_execution) {
        try {
            double realtime_price_amount = api_manager.get_current_price(config.trading_mode.primary_symbol);
            if (realtime_price_amount > 0.0) {
                entry_price_amount = realtime_price_amount;
            }
        } catch (const std::exception& realtime_price_api_exception_error) {
            // Fall back to using processed data price, error will be logged by coordinator
            // Let exception propagate if caller needs to handle it differently
            throw std::runtime_error("API error fetching realtime price in calculate_exit_targets: " + std::string(realtime_price_api_exception_error.what()));
        } catch (...) {
            throw std::runtime_error("Unknown API error fetching realtime price in calculate_exit_targets");
        }
    }
    
    return compute_exit_targets(ExitTargetsRequest(
        (order_side_input == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string, 
        entry_price_amount, 
        position_sizing_input.risk_amount, 
        config.strategy
    ));
}

// Order timing methods for wash trade prevention
bool OrderExecutionLogic::can_place_order_now() const {
    // Validate data_sync is properly initialized
    if (!data_sync_ptr || !data_sync_ptr->last_order_timestamp) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();

    // Read the timestamp once to avoid race condition
    auto last_order = data_sync_ptr->last_order_timestamp->load();

    // Check if timestamp is properly initialized (not default-constructed)
    if (last_order == std::chrono::steady_clock::time_point{}) {
        return true;
    }

    // Check if enough time has passed
    auto time_since_last_order = std::chrono::duration_cast<std::chrono::seconds>(now - last_order);
    int min_interval = config.timing.minimum_interval_between_orders_seconds;
    int elapsed_seconds = time_since_last_order.count();

    return elapsed_seconds >= min_interval;
}

void OrderExecutionLogic::update_last_order_timestamp() {
    // Validate data_sync is properly initialized
    if (!data_sync_ptr || !data_sync_ptr->last_order_timestamp) {
        throw std::runtime_error("Data sync not initialized - cannot update last order timestamp");
    }

    data_sync_ptr->last_order_timestamp->store(std::chrono::steady_clock::now());
}

void OrderExecutionLogic::set_data_sync_reference(DataSyncReferences* data_sync_reference) {
    data_sync_ptr = data_sync_reference;
}

bool OrderExecutionLogic::is_flat_position(int position_quantity) const {
    return position_quantity == 0;
}

bool OrderExecutionLogic::should_cancel_existing_orders() const {
    return true;
}

bool OrderExecutionLogic::validate_trade_feasibility(const PositionSizing& position_sizing_input, double buying_power_amount, double current_price_amount) const {
    if (position_sizing_input.quantity <= 0) {
        return false;
    }
    
    double position_value_amount = position_sizing_input.quantity * current_price_amount;
    double required_buying_power_amount = position_value_amount * config.strategy.buying_power_validation_safety_margin;
    
    return buying_power_amount >= required_buying_power_amount;
}

bool OrderExecutionLogic::handle_market_close_positions(const ProcessedData& processed_data_input) {
    try {
        if (api_manager.is_market_open(config.trading_mode.primary_symbol)) {
            return false;
        }
    } catch (const std::exception& market_open_api_exception_error) {
        // If API check fails, assume market is closed and proceed with position closure
        // Error will be logged by coordinator
    } catch (...) {
        // If unknown error, assume market is closed and proceed with position closure
    }
    
    int current_position_quantity = processed_data_input.pos_details.position_quantity;
    if (current_position_quantity == 0) {
        return false;
    }
    
    int market_close_grace_period_minutes = config.timing.market_close_grace_period_minutes;
    if (market_close_grace_period_minutes <= 0) {
        throw std::runtime_error("Invalid market close grace period - must be positive");
    }
    
    try {
        api_manager.close_position(config.trading_mode.primary_symbol, current_position_quantity);
        return true;
    } catch (const std::exception& close_position_api_exception_error) {
        throw std::runtime_error("API error closing position: " + std::string(close_position_api_exception_error.what()));
    } catch (...) {
        throw std::runtime_error("Unknown API error closing position");
    }
}

} // namespace Core
} // namespace AlpacaTrader
