#include "order_execution_logic.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "logging/logs/trading_logs.hpp"
#include "json/json.hpp"
#include <thread>
#include <chrono>
#include <stdexcept>
#include <cmath>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

namespace AlpacaTrader {
namespace Core {
using namespace AlpacaTrader::Logging;

OrderExecutionLogic::OrderExecutionLogic(const OrderExecutionLogicConstructionParams& construction_params)
    : api_manager(construction_params.api_manager_ref), account_manager(construction_params.account_manager_ref), 
      config(construction_params.system_config), data_sync_ptr(construction_params.data_sync_ptr) {}

void OrderExecutionLogic::execute_trade(const ProcessedData& processed_data_input, int current_position_quantity, const PositionSizing& position_sizing_input, const SignalDecision& signal_decision_input) {
    try {
        if (!validate_order_parameters(processed_data_input, position_sizing_input)) {
            throw std::runtime_error("Order validation failed - aborting trade execution");
        }
    } catch (const std::runtime_error& validation_error) {
        // Re-throw with detailed error message
        throw std::runtime_error("Order validation failed: " + std::string(validation_error.what()));
    }
    
    if (processed_data_input.curr.close_price <= 0.0) {
        throw std::runtime_error("Invalid price data - price is zero or negative");
    }
    
    if (position_sizing_input.quantity <= 0.0) {
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

    // Detect crypto mode: symbol contains "/" (e.g., "BTC/USD")
    bool is_crypto_mode = config.trading_mode.primary_symbol.find('/') != std::string::npos;
    
    if (signal_decision_input.buy) {
        execute_order(OrderSide::Buy, processed_data_input, current_position_quantity, position_sizing_input);
    } else if (signal_decision_input.sell) {
        // ALPACA DOCS: Cryptocurrencies cannot be sold short
        // For crypto: buy-and-hold strategy - sell signal means close entire position immediately
        // For stocks: allow short selling if configured
        if (is_crypto_mode) {
            // CRITICAL: For crypto sell signals, fetch the actual current position from account
            // This ensures we sell the entire actual position, not a potentially stale value
            // CRITICAL: Handle fractional crypto quantities - fetch directly from API as double
            // This ensures we get the exact current position quantity, including fractional amounts
            double actual_position_quantity = 0.0;
            bool position_found_in_api = false;
            try {
                std::string positions_json = api_manager.get_positions();
                if (!positions_json.empty()) {
                    json positions_data = json::parse(positions_json);
                    // Check if positions_data is an array
                    if (positions_data.is_array()) {
                        for (const auto& position : positions_data) {
                            if (position.contains("symbol") && position["symbol"].get<std::string>() == config.trading_mode.primary_symbol) {
                                if (position.contains("qty")) {
                                    position_found_in_api = true;
                                    if (position["qty"].is_string()) {
                                        // Crypto quantities are often returned as strings (e.g., "0.00099645")
                                        actual_position_quantity = std::stod(position["qty"].get<std::string>());
                                    } else if (position["qty"].is_number()) {
                                        // Some APIs return as number - use double for fractional crypto quantities
                                        actual_position_quantity = position["qty"].get<double>();
                                    } else {
                                        // Fallback: use cached value if qty field is unexpected type
                                        actual_position_quantity = static_cast<double>(current_position_quantity);
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
                
                // If position not found in API response, fall back to cached value
                if (!position_found_in_api) {
                    actual_position_quantity = static_cast<double>(current_position_quantity);
                    std::ostringstream fallback_log_stream;
                    fallback_log_stream << "Position not found in API response for " << config.trading_mode.primary_symbol 
                                       << ", using cached quantity: " << current_position_quantity;
                    log_message(fallback_log_stream.str(), "");
                }
            } catch (const std::exception& position_fetch_exception_error) {
                // Fallback to using the passed current_position_quantity if fetch fails
                actual_position_quantity = static_cast<double>(current_position_quantity);
                std::ostringstream fallback_log_stream;
                fallback_log_stream << "Warning: Failed to fetch fresh position from account, using cached value. Error: " 
                                   << std::string(position_fetch_exception_error.what());
                log_message(fallback_log_stream.str(), "");
            }
            
            if (actual_position_quantity <= 0.0) {
                // No position to close - silently skip (crypto cannot be shorted)
                return;
            }
            
            // CRITICAL: Crypto sell signal - close entire position immediately with market order
            // Create PositionSizing with full actual position quantity for immediate market order execution
            PositionSizing full_position_sizing = position_sizing_input;
            full_position_sizing.quantity = actual_position_quantity;
            
            // Log the full position closure with actual fetched quantity
            std::ostringstream closure_log_stream;
            closure_log_stream << "Crypto sell signal detected - closing entire position immediately. "
                               << "Fetched position quantity: " << std::fixed << std::setprecision(8) << actual_position_quantity
                               << " | Symbol: " << config.trading_mode.primary_symbol;
            log_message(closure_log_stream.str(), "");
            
            // Execute market order directly to close entire position (bypass bracket order logic)
            execute_market_order(OrderSide::Sell, processed_data_input, full_position_sizing);
            
            // Update the last order timestamp after successful order placement
            update_last_order_timestamp();
        } else {
            // Stocks: allow short selling
            if (current_position_quantity == 0) {
                // Opening new short position - allowed for stocks
                execute_order(OrderSide::Sell, processed_data_input, current_position_quantity, position_sizing_input);
            } else if (current_position_quantity > 0) {
                // Closing long position
                execute_order(OrderSide::Sell, processed_data_input, current_position_quantity, position_sizing_input);
            } else {
                // Closing short position (buy to cover)
                execute_order(OrderSide::Buy, processed_data_input, current_position_quantity, position_sizing_input);
            }
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
    
    // Detect crypto mode: symbol contains "/" (e.g., "BTC/USD")
    bool is_crypto_mode = config.trading_mode.primary_symbol.find('/') != std::string::npos;
    
    if (current_position_quantity == 0) {
        ExitTargets exit_targets_result = calculate_exit_targets(order_side_input, processed_data_input, position_sizing_input);
        
        // ALPACA DOCS: Bracket orders are NOT supported for crypto
        // For crypto: use separate orders + monitoring to simulate bracket order behavior
        // For stocks: use native bracket orders
        if (is_crypto_mode) {
            execute_crypto_bracket_simulation(order_side_input, processed_data_input, position_sizing_input, exit_targets_result);
        } else {
            execute_bracket_order(order_side_input, processed_data_input, position_sizing_input, exit_targets_result);
        }
    } else {
        // Closing or adjusting existing position - use market order for speed
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
    double quantity_value = position_sizing_input.quantity;
    double entry_price_amount = processed_data_input.curr.close_price;
    
    // Detect crypto mode: symbol contains "/" (e.g., "BTC/USD")
    bool is_crypto_mode = config.trading_mode.primary_symbol.find('/') != std::string::npos;
    
    if (symbol_string.empty()) {
        throw std::runtime_error("Symbol is required for bracket order");
    }
    
    if (quantity_value <= 0.0) {
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
    
    // ALPACA DOCS: Crypto cannot be sold short - only allow sell to close existing positions
    // For crypto, "sell" should only be used when there's an existing long position
    if (is_crypto_mode && order_side_input == OrderSide::Sell) {
        // This is handled by execute_trade - should not reach here for opening new shorts
        // But validate anyway for safety
        log_message("WARNING: Attempting sell order for crypto - ensure this is to close existing position", "");
    }
    
    bool order_placed_status = false;
    int max_retry_attempts = config.strategy.max_retries;
    int retry_delay_milliseconds = config.strategy.retry_delay_ms;
    
    for (int retry_attempt_number = 1; retry_attempt_number <= max_retry_attempts && !order_placed_status; ++retry_attempt_number) {
        try {
            json bracket_order_json;
            bracket_order_json["symbol"] = symbol_string;
            std::ostringstream qty_stream;
            qty_stream << std::fixed << std::setprecision(8) << quantity_value;
            bracket_order_json["qty"] = qty_stream.str();
            bracket_order_json["side"] = order_side_string;
            bracket_order_json["type"] = "market";
            
            // ALPACA DOCS: Crypto only supports "gtc" and "ioc" time_in_force values (NOT "day")
            // Use "gtc" (Good Till Canceled) for crypto, "day" for stocks
            if (is_crypto_mode) {
                bracket_order_json["time_in_force"] = "gtc";
            } else {
                bracket_order_json["time_in_force"] = "day";
            }
            
            bracket_order_json["order_class"] = "bracket";
            
            double stop_loss_price = exit_targets_input.stop_loss;
            double take_profit_price = exit_targets_input.take_profit;
            
            // ALPACA DOCS: For sell orders, stop_loss.stop_price must be >= base_price + 0.01
            // For crypto, Alpaca uses a "base_price" which may differ from our entry price
            // Adjust stop loss for sell orders to ensure it meets Alpaca's minimum requirement
            if (order_side_input == OrderSide::Sell && is_crypto_mode) {
                // For sell orders, ensure stop loss is at least entry_price + buffer + 0.01
                // This accounts for Alpaca's base_price potentially being higher than our entry
                double min_stop_for_sell = entry_price_amount + 0.01;
                if (stop_loss_price < min_stop_for_sell) {
                    std::ostringstream adjustment_log_stream;
                    adjustment_log_stream << "Adjusting stop loss for sell order - Original: $" << std::fixed << std::setprecision(2) << stop_loss_price
                                         << " | Minimum required: $" << std::fixed << std::setprecision(2) << min_stop_for_sell
                                         << " | Alpaca rule: stop_loss >= base_price + 0.01";
                    log_message(adjustment_log_stream.str(), "");
                    stop_loss_price = min_stop_for_sell;
                }
            }
            
            bracket_order_json["stop_loss"] = json::object();
            bracket_order_json["stop_loss"]["stop_price"] = std::to_string(stop_loss_price);
            bracket_order_json["stop_loss"]["limit_price"] = std::to_string(stop_loss_price);
            
            bracket_order_json["take_profit"] = json::object();
            bracket_order_json["take_profit"]["limit_price"] = std::to_string(take_profit_price);
            
            // Log order details before submission for debugging
            std::ostringstream order_details_stream;
            order_details_stream << "Submitting bracket order - Symbol: " << symbol_string 
                                 << " | Side: " << order_side_string 
                                 << " | Qty: " << std::fixed << std::setprecision(8) << quantity_value
                                 << " | Entry: $" << std::fixed << std::setprecision(2) << entry_price_amount
                                 << " | Stop Loss: $" << std::fixed << std::setprecision(2) << stop_loss_price
                                 << " | Take Profit: $" << std::fixed << std::setprecision(2) << take_profit_price
                                 << " | Time in Force: " << (is_crypto_mode ? "gtc" : "day")
                                 << " | Crypto Mode: " << (is_crypto_mode ? "YES" : "NO");
            log_message(order_details_stream.str(), "");
            
            std::string order_json_string = bracket_order_json.dump();
            
            // Log the JSON payload for debugging
            std::ostringstream json_payload_stream;
            json_payload_stream << "Order JSON Payload: " << order_json_string;
            log_message(json_payload_stream.str(), "");
            
            std::string api_response = api_manager.place_order(order_json_string);
            
            // Parse API response to check for errors or success
            try {
                json response_json = json::parse(api_response);
                
                // Check if response contains an error (Alpaca error format)
                if (response_json.contains("code") || (response_json.contains("message") && !response_json.contains("id"))) {
                    // This is an error response - parse all error fields
                    int error_code = response_json.value("code", 0);
                    std::string error_message = response_json.value("message", "");
                    std::string base_price_str = response_json.value("base_price", "");
                    
                    // Build comprehensive error message
                    std::ostringstream error_details_stream;
                    error_details_stream << "ALPACA API ERROR - Order REJECTED: " << error_message;
                    if (error_code != 0) {
                        error_details_stream << " | Error Code: " << error_code;
                    }
                    if (!base_price_str.empty()) {
                        error_details_stream << " | Alpaca Base Price: $" << base_price_str;
                        error_details_stream << " | Our Entry Price: $" << std::fixed << std::setprecision(2) << entry_price_amount;
                        error_details_stream << " | Our Stop Loss: $" << std::fixed << std::setprecision(2) << stop_loss_price;
                        
                        // For sell orders, explain the stop loss requirement and suggest fix
                        if (order_side_input == OrderSide::Sell) {
                            try {
                                double base_price_value = std::stod(base_price_str);
                                double required_min_stop = base_price_value + 0.01;
                                error_details_stream << " | Required Min Stop Loss: $" << std::fixed << std::setprecision(2) << required_min_stop;
                                if (stop_loss_price < required_min_stop) {
                                    double adjustment_needed = required_min_stop - stop_loss_price;
                                    error_details_stream << " | ISSUE: Stop loss must be >= base_price + 0.01";
                                    error_details_stream << " | Adjustment needed: +$" << std::fixed << std::setprecision(2) << adjustment_needed;
                                    error_details_stream << " | ALPACA RULE: For sell orders, stop_loss.stop_price must be >= base_price + 0.01";
                                }
                            } catch (const std::exception& parse_error) {
                                error_details_stream << " | Could not parse base_price for validation";
                            }
                        }
                        
                        // Check if this is a bracket order validation error
                        if (error_message.find("stop_loss") != std::string::npos || error_message.find("bracket") != std::string::npos) {
                            error_details_stream << " | NOTE: Bracket orders may not be fully supported for crypto. Consider using separate limit orders.";
                        }
                    }
                    
                    // Log full API response for debugging
                    std::ostringstream api_response_stream;
                    api_response_stream << "Full Alpaca API Error Response: " << api_response;
                    log_message(api_response_stream.str(), "");
                    
                    // Log order details that failed
                    std::ostringstream failed_order_stream;
                    failed_order_stream << "Failed Order Details - Symbol: " << symbol_string 
                                        << " | Side: " << order_side_string 
                                        << " | Qty: " << std::fixed << std::setprecision(8) << quantity_value
                                        << " | Entry: $" << std::fixed << std::setprecision(2) << entry_price_amount
                                        << " | Stop: $" << std::fixed << std::setprecision(2) << stop_loss_price
                                        << " | TP: $" << std::fixed << std::setprecision(2) << take_profit_price;
                    log_message(failed_order_stream.str(), "");
                    
                    throw std::runtime_error(error_details_stream.str());
                }
                
                // Success response - extract order details
                if (response_json.contains("id")) {
                    // Safe extraction helper: handles null values in JSON
                    auto safe_get_string = [](const json& json_obj, const std::string& key, const std::string& default_value) -> std::string {
                        if (!json_obj.contains(key)) {
                            return default_value;
                        }
                        if (json_obj[key].is_null()) {
                            return default_value;
                        }
                        if (json_obj[key].is_string()) {
                            return json_obj[key].get<std::string>();
                        }
                        // For numeric types, convert to string
                        if (json_obj[key].is_number()) {
                            return std::to_string(json_obj[key].get<double>());
                        }
                        return default_value;
                    };
                    
                    std::string order_id = safe_get_string(response_json, "id", "");
                    std::string order_status = safe_get_string(response_json, "status", "");
                    std::string filled_qty = safe_get_string(response_json, "filled_qty", "0");
                    std::string filled_avg_price = safe_get_string(response_json, "filled_avg_price", "");
                    std::string submitted_at = safe_get_string(response_json, "submitted_at", "");
                    std::string order_type = safe_get_string(response_json, "type", "");
                    std::string time_in_force_actual = safe_get_string(response_json, "time_in_force", "");
                    
                    order_placed_status = true;
                    
                    // Log actual API confirmation with all details using table format
                    TradingLogs::log_order_accepted("Market Order", symbol_string, order_side_string, quantity_value,
                                                     order_id, order_status, filled_qty, filled_avg_price,
                                                     submitted_at, exit_targets_input.stop_loss, exit_targets_input.take_profit);
                    
                    // Log full API response for debugging
                    TradingLogs::log_api_response_full(api_response);
                } else {
                    // Unexpected response format
                    std::ostringstream unexpected_error_stream;
                    unexpected_error_stream << "Unexpected API response format - no order ID or error code found. "
                                           << "This may indicate bracket orders are not supported for crypto. "
                                           << "Response: " << api_response;
                    log_message(unexpected_error_stream.str(), "");
                    throw std::runtime_error(unexpected_error_stream.str());
                }
            } catch (const std::runtime_error& runtime_exception_error) {
                // Re-throw runtime errors (API errors)
                throw;
            } catch (const std::exception& parse_exception_error) {
                // If parsing fails, log the raw response and throw
                std::ostringstream error_message_stream;
                error_message_stream << "Failed to parse API response: " 
                                     << std::string(parse_exception_error.what()) 
                                     << " | Raw response: " << api_response;
                throw std::runtime_error(error_message_stream.str());
            }
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
    double quantity_value = position_sizing_input.quantity;
    double current_price_amount = processed_data_input.curr.close_price;
    
    if (symbol_string.empty()) {
        throw std::runtime_error("Symbol is required for market order");
    }
    
    if (quantity_value <= 0.0) {
        throw std::runtime_error("Quantity must be positive for market order");
    }
    
    if (current_price_amount <= 0.0 || !std::isfinite(current_price_amount)) {
        throw std::runtime_error("Invalid current price for market order");
    }
    
    // Detect crypto mode: symbol contains "/" (e.g., "BTC/USD")
    bool is_crypto_mode = config.trading_mode.primary_symbol.find('/') != std::string::npos;
    
    json market_order_json;
    market_order_json["symbol"] = symbol_string;
    std::ostringstream qty_stream;
    qty_stream << std::fixed << std::setprecision(8) << quantity_value;
    market_order_json["qty"] = qty_stream.str();
    market_order_json["side"] = order_side_string;
    market_order_json["type"] = "market";
    
    // ALPACA DOCS: Crypto only supports "gtc" and "ioc" time_in_force values (NOT "day")
    // Use "gtc" (Good Till Canceled) for crypto, "day" for stocks
    if (is_crypto_mode) {
        market_order_json["time_in_force"] = "gtc";
    } else {
        market_order_json["time_in_force"] = "day";
    }
    
    // Log order details before submission using table format
    TradingLogs::log_order_submission("Market Order", symbol_string, order_side_string, quantity_value, 
                                     current_price_amount, (is_crypto_mode ? "gtc" : "day"), is_crypto_mode);
    
    std::string order_json_string = market_order_json.dump();
    std::string api_response = api_manager.place_order(order_json_string);
    
    // Parse API response to check for errors or success
    try {
        json response_json = json::parse(api_response);
        
        // Check if response contains an error (Alpaca error format)
        if (response_json.contains("code") || (response_json.contains("message") && !response_json.contains("id"))) {
            // This is an error response - parse all error fields
            int error_code = response_json.value("code", 0);
            std::string error_message = response_json.value("message", "");
            std::string base_price_str = response_json.value("base_price", "");
            
            // Build comprehensive error message
            std::ostringstream error_details_stream;
            error_details_stream << "ALPACA API ERROR - Order REJECTED: " << error_message;
            if (error_code != 0) {
                error_details_stream << " | Error Code: " << error_code;
            }
            if (!base_price_str.empty()) {
                error_details_stream << " | Alpaca Base Price: $" << base_price_str;
                error_details_stream << " | Our Price: $" << std::fixed << std::setprecision(2) << current_price_amount;
            }
            
            // Log full API response for debugging
            std::ostringstream api_response_stream;
            api_response_stream << "Full Alpaca API Error Response: " << api_response;
            log_message(api_response_stream.str(), "");
            
            // Log order details that failed
            std::ostringstream failed_order_stream;
            failed_order_stream << "Failed Order Details - Symbol: " << symbol_string 
                                << " | Side: " << order_side_string 
                                << " | Qty: " << std::fixed << std::setprecision(8) << quantity_value
                                << " | Price: $" << std::fixed << std::setprecision(2) << current_price_amount;
            log_message(failed_order_stream.str(), "");
            
            throw std::runtime_error(error_details_stream.str());
        }
        
        // Success response - extract order details
        if (response_json.contains("id")) {
            // Safe extraction helper: handles null values in JSON
            auto safe_get_string = [](const json& json_obj, const std::string& key, const std::string& default_value) -> std::string {
                if (!json_obj.contains(key)) {
                    return default_value;
                }
                if (json_obj[key].is_null()) {
                    return default_value;
                }
                if (json_obj[key].is_string()) {
                    return json_obj[key].get<std::string>();
                }
                // For numeric types, convert to string
                if (json_obj[key].is_number()) {
                    return std::to_string(json_obj[key].get<double>());
                }
                return default_value;
            };
            
            std::string order_id = safe_get_string(response_json, "id", "");
            std::string order_status = safe_get_string(response_json, "status", "");
            std::string filled_qty = safe_get_string(response_json, "filled_qty", "0");
            std::string filled_avg_price = safe_get_string(response_json, "filled_avg_price", "");
            std::string submitted_at = safe_get_string(response_json, "submitted_at", "");
            std::string order_type = safe_get_string(response_json, "type", "");
            std::string time_in_force_actual = safe_get_string(response_json, "time_in_force", "");
            
            // Log actual API confirmation with all details using table format
            TradingLogs::log_order_accepted("Market Order", symbol_string, order_side_string, quantity_value,
                                             order_id, order_status, filled_qty, filled_avg_price,
                                             submitted_at, 0.0, 0.0);
            
            // Log full API response for debugging
            TradingLogs::log_api_response_full(api_response);
        } else {
            // Unexpected response format
            std::ostringstream unexpected_error_stream;
            unexpected_error_stream << "Unexpected API response format - no order ID or error code found. Response: " << api_response;
            log_message(unexpected_error_stream.str(), "");
            throw std::runtime_error(unexpected_error_stream.str());
        }
    } catch (const std::runtime_error& runtime_exception_error) {
        // Re-throw runtime errors (API errors)
        throw;
    } catch (const std::exception& parse_exception_error) {
        // If parsing fails, log the raw response and throw
        std::ostringstream error_message_stream;
        error_message_stream << "Failed to parse API response: " 
                             << std::string(parse_exception_error.what()) 
                             << " | Raw response: " << api_response;
        throw std::runtime_error(error_message_stream.str());
    }
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
        throw std::runtime_error("Validation failed: Price <= 0.0");
    }
    
    if (position_sizing_input.quantity <= 0.0) {
        throw std::runtime_error("Validation failed: Quantity <= 0.0");
    }
    
    if (position_sizing_input.risk_amount <= 0.0) {
        throw std::runtime_error("Validation failed: Risk amount <= 0.0");
    }
    
    // Detect crypto mode: symbol contains "/" (e.g., "BTC/USD")
    bool is_crypto_mode = config.trading_mode.primary_symbol.find('/') != std::string::npos;
    
    // Additional validation for order rejection prevention using config values
    // For crypto, skip share quantity check (crypto uses fractional quantities)
    // For stocks, check against maximum share quantity
    if (!is_crypto_mode) {
        if (position_sizing_input.quantity > config.strategy.maximum_share_quantity_per_single_trade) {
            throw std::runtime_error("Validation failed: Quantity (" + std::to_string(position_sizing_input.quantity) + 
                ") exceeds maximum (" + std::to_string(config.strategy.maximum_share_quantity_per_single_trade) + ")");
        }
    }
    
    // Validate price is within configured range
    // For crypto, skip price range check (crypto prices can be much higher than stocks)
    // For stocks, validate price range
    if (!is_crypto_mode) {
        if (processed_data_input.curr.close_price < config.strategy.minimum_acceptable_price_for_signals || 
            processed_data_input.curr.close_price > config.strategy.maximum_acceptable_price_for_signals) {
            throw std::runtime_error("Validation failed: Price (" + std::to_string(processed_data_input.curr.close_price) + 
                ") outside range [" + std::to_string(config.strategy.minimum_acceptable_price_for_signals) + ", " + 
                std::to_string(config.strategy.maximum_acceptable_price_for_signals) + "]");
        }
    }
    
    // Check if order value exceeds configured maximum
    // This check applies to both crypto and stocks
    // COMPLIANCE: No defaults allowed - explicit configuration required; fail hard if invalid
    double order_value_amount = processed_data_input.curr.close_price * position_sizing_input.quantity;
    
    // Determine the maximum order value limit
    // Use the stricter of the two limits: strategy.maximum_dollar_value_per_trade ($100) 
    // or strategy.maximum_dollar_value_per_single_trade ($10,000)
    double max_order_value = 0.0;
    
    if (config.strategy.maximum_dollar_value_per_trade > 0.0) {
        max_order_value = config.strategy.maximum_dollar_value_per_trade;
    }
    
    if (config.strategy.maximum_dollar_value_per_single_trade > 0.0) {
        if (max_order_value > 0.0) {
            // Use the stricter (lower) of the two limits
            max_order_value = std::min(max_order_value, config.strategy.maximum_dollar_value_per_single_trade);
        } else {
            max_order_value = config.strategy.maximum_dollar_value_per_single_trade;
        }
    }
    
    // COMPLIANCE: Fail hard if configuration is invalid - no defaults allowed
    if (max_order_value <= 0.0) {
        throw std::runtime_error("Validation failed: Invalid configuration - both maximum_dollar_value_per_trade and maximum_dollar_value_per_single_trade are uninitialized or zero. Order value validation cannot proceed.");
    }
    
    // Validate order value against configured maximum
    // Use epsilon tolerance to account for floating-point precision issues
    // Allow orders that are exactly at the limit or within 0.01% tolerance
    const double validation_epsilon = std::max(0.01, max_order_value * 0.0001); // At least 1 cent, or 0.01% of max
    if (order_value_amount > max_order_value + validation_epsilon) {
        throw std::runtime_error("Validation failed: Order value ($" + std::to_string(order_value_amount) + 
            ") exceeds maximum ($" + std::to_string(max_order_value) + ")");
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

// Order type selection logic - per Alpaca docs
// ALPACA DOCS: https://docs.alpaca.markets/docs/orders-at-alpaca
// - Market orders: Execute immediately, fastest fill, price slippage risk
// - Limit orders: Price protection, may not fill, use when price is critical
// - Stop-limit orders: Risk management, trigger at stop price then limit execution
OrderExecutionLogic::OrderType OrderExecutionLogic::select_order_type(OrderSide order_side_input, const ProcessedData& processed_data_input, const PositionSizing& position_sizing_input, bool has_stop_targets) const {
    // Detect crypto mode - affects order type selection
    bool is_crypto_mode = config.trading_mode.primary_symbol.find('/') != std::string::npos;
    
    // For closing positions: use market orders for speed
    // For new positions with stop targets: prefer limit orders for price control
    // For high volatility: use stop-limit orders for risk management
    
    // Calculate volatility metrics
    double volatility_ratio = (processed_data_input.avg_atr > 0.0) ? (processed_data_input.atr / processed_data_input.avg_atr) : 1.0;
    double price_change_pct = (processed_data_input.prev.close_price > 0.0) 
        ? std::abs((processed_data_input.curr.close_price - processed_data_input.prev.close_price) / processed_data_input.prev.close_price) * 100.0 
        : 0.0;
    
    // Consider position size for order type selection - larger positions may need limit orders for price control
    double position_value_dollars = position_sizing_input.quantity * processed_data_input.curr.close_price;
    bool is_large_position = position_value_dollars > 10000.0;
    
    // For crypto: prefer limit orders more often due to 24/7 market and potential slippage
    // For buy orders: limit orders help control entry price
    // For sell orders: limit orders help control exit price
    bool is_buy_order = (order_side_input == OrderSide::Buy);
    bool prefer_limit_for_crypto = is_crypto_mode && (is_buy_order || is_large_position);
    
    // High volatility scenario: use stop-limit to protect against adverse moves
    if (volatility_ratio > 1.5 || price_change_pct > 1.0) {
        if (has_stop_targets) {
            return OrderType::StopLimit;
        }
        if (prefer_limit_for_crypto) {
            return OrderType::Limit;
        }
        return OrderType::Limit;
    }
    
    // Normal volatility: use limit orders for price control when opening new positions
    if (has_stop_targets) {
        return OrderType::Limit;
    }
    
    // Crypto with large positions: prefer limit orders to control slippage
    if (prefer_limit_for_crypto) {
        return OrderType::Limit;
    }
    
    // Default: market orders for fastest execution
    return OrderType::Market;
}

// Execute limit order
void OrderExecutionLogic::execute_limit_order(OrderSide order_side_input, const ProcessedData& processed_data_input, const PositionSizing& position_sizing_input, double limit_price) {
    // Validate processed_data_input is used for logging and validation
    if (processed_data_input.curr.close_price <= 0.0) {
        throw std::runtime_error("Invalid price data in processed_data_input for limit order");
    }
    std::string order_side_string = (order_side_input == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string;
    std::string symbol_string = config.trading_mode.primary_symbol;
    double quantity_value = position_sizing_input.quantity;
    bool is_crypto_mode = config.trading_mode.primary_symbol.find('/') != std::string::npos;
    
    if (symbol_string.empty()) {
        throw std::runtime_error("Symbol is required for limit order");
    }
    
    if (quantity_value <= 0.0) {
        throw std::runtime_error("Quantity must be positive for limit order");
    }
    
    if (limit_price <= 0.0 || !std::isfinite(limit_price)) {
        throw std::runtime_error("Invalid limit price for limit order");
    }
    
    json limit_order_json;
    limit_order_json["symbol"] = symbol_string;
    std::ostringstream qty_stream;
    qty_stream << std::fixed << std::setprecision(8) << quantity_value;
    limit_order_json["qty"] = qty_stream.str();
    limit_order_json["side"] = order_side_string;
    limit_order_json["type"] = "limit";
    limit_order_json["limit_price"] = std::to_string(limit_price);
    
    // ALPACA DOCS: Crypto only supports "gtc" and "ioc" time_in_force values
    if (is_crypto_mode) {
        limit_order_json["time_in_force"] = "gtc";
    } else {
        limit_order_json["time_in_force"] = "day";
    }
    
    // Log order details before submission using table format
    TradingLogs::log_order_submission("Limit Order", symbol_string, order_side_string, quantity_value,
                                     processed_data_input.curr.close_price, (is_crypto_mode ? "gtc" : "day"), is_crypto_mode);
    
    std::string order_json_string = limit_order_json.dump();
    std::string api_response = api_manager.place_order(order_json_string);
    
    // Parse and log response (same pattern as market order)
    try {
        json response_json = json::parse(api_response);
        
        if (response_json.contains("code") || (response_json.contains("message") && !response_json.contains("id"))) {
            int error_code = response_json.value("code", 0);
            std::string error_message = response_json.value("message", "");
            
            std::ostringstream error_details_stream;
            error_details_stream << "ALPACA API ERROR - Limit Order REJECTED: " << error_message;
            if (error_code != 0) {
                error_details_stream << " | Error Code: " << error_code;
            }
            
            log_message("Full Alpaca API Error Response: " + api_response, "");
            throw std::runtime_error(error_details_stream.str());
        }
        
        if (response_json.contains("id")) {
            // Safe extraction helper: handles null values in JSON
            auto safe_get_string = [](const json& json_obj, const std::string& key, const std::string& default_value) -> std::string {
                if (!json_obj.contains(key)) {
                    return default_value;
                }
                if (json_obj[key].is_null()) {
                    return default_value;
                }
                if (json_obj[key].is_string()) {
                    return json_obj[key].get<std::string>();
                }
                if (json_obj[key].is_number()) {
                    return std::to_string(json_obj[key].get<double>());
                }
                return default_value;
            };
            
            std::string order_id = safe_get_string(response_json, "id", "");
            std::string order_status = safe_get_string(response_json, "status", "");
            std::string filled_qty_value = safe_get_string(response_json, "filled_qty", "0");
            std::string filled_avg_price_value = safe_get_string(response_json, "filled_avg_price", "");
            std::string submitted_at_value = safe_get_string(response_json, "submitted_at", "");
            
            // Log order acceptance using table format
            TradingLogs::log_order_accepted("Limit Order", symbol_string, order_side_string, quantity_value,
                                             order_id, order_status, filled_qty_value, filled_avg_price_value,
                                             submitted_at_value, 0.0, limit_price);
            
            // Log full API response for debugging
            TradingLogs::log_api_response_full(api_response);
        } else {
            throw std::runtime_error("Unexpected API response format - no order ID or error code found. Response: " + api_response);
        }
    } catch (const std::runtime_error& runtime_exception_error) {
        throw;
    } catch (const std::exception& parse_exception_error) {
        throw std::runtime_error("Failed to parse API response: " + std::string(parse_exception_error.what()) + " | Raw response: " + api_response);
    }
}

// Execute stop-limit order
void OrderExecutionLogic::execute_stop_limit_order(OrderSide order_side_input, const ProcessedData& processed_data_input, const PositionSizing& position_sizing_input, double stop_price, double limit_price) {
    // Validate processed_data_input is used for logging and validation
    if (processed_data_input.curr.close_price <= 0.0) {
        throw std::runtime_error("Invalid price data in processed_data_input for stop-limit order");
    }
    std::string order_side_string = (order_side_input == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string;
    std::string symbol_string = config.trading_mode.primary_symbol;
    double quantity_value = position_sizing_input.quantity;
    bool is_crypto_mode = config.trading_mode.primary_symbol.find('/') != std::string::npos;
    
    if (symbol_string.empty()) {
        throw std::runtime_error("Symbol is required for stop-limit order");
    }
    
    if (quantity_value <= 0.0) {
        throw std::runtime_error("Quantity must be positive for stop-limit order");
    }
    
    if (stop_price <= 0.0 || !std::isfinite(stop_price)) {
        throw std::runtime_error("Invalid stop price for stop-limit order");
    }
    
    if (limit_price <= 0.0 || !std::isfinite(limit_price)) {
        throw std::runtime_error("Invalid limit price for stop-limit order");
    }
    
    json stop_limit_order_json;
    stop_limit_order_json["symbol"] = symbol_string;
    std::ostringstream qty_stream;
    qty_stream << std::fixed << std::setprecision(8) << quantity_value;
    stop_limit_order_json["qty"] = qty_stream.str();
    stop_limit_order_json["side"] = order_side_string;
    stop_limit_order_json["type"] = "stop_limit";
    stop_limit_order_json["stop_price"] = std::to_string(stop_price);
    stop_limit_order_json["limit_price"] = std::to_string(limit_price);
    
    // ALPACA DOCS: Crypto only supports "gtc" and "ioc" time_in_force values
    if (is_crypto_mode) {
        stop_limit_order_json["time_in_force"] = "gtc";
    } else {
        stop_limit_order_json["time_in_force"] = "day";
    }
    
    // Log order details before submission using table format
    TradingLogs::log_order_submission("Stop-Limit Order", symbol_string, order_side_string, quantity_value,
                                     processed_data_input.curr.close_price, (is_crypto_mode ? "gtc" : "day"), is_crypto_mode);
    
    std::string order_json_string = stop_limit_order_json.dump();
    std::string api_response = api_manager.place_order(order_json_string);
    
    // Parse and log response
    try {
        json response_json = json::parse(api_response);
        
        if (response_json.contains("code") || (response_json.contains("message") && !response_json.contains("id"))) {
            int error_code = response_json.value("code", 0);
            std::string error_message = response_json.value("message", "");
            
            std::ostringstream error_details_stream;
            error_details_stream << "ALPACA API ERROR - Stop-Limit Order REJECTED: " << error_message;
            if (error_code != 0) {
                error_details_stream << " | Error Code: " << error_code;
            }
            
            log_message("Full Alpaca API Error Response: " + api_response, "");
            throw std::runtime_error(error_details_stream.str());
        }
        
        if (response_json.contains("id")) {
            // Safe extraction helper: handles null values in JSON
            auto safe_get_string = [](const json& json_obj, const std::string& key, const std::string& default_value) -> std::string {
                if (!json_obj.contains(key)) {
                    return default_value;
                }
                if (json_obj[key].is_null()) {
                    return default_value;
                }
                if (json_obj[key].is_string()) {
                    return json_obj[key].get<std::string>();
                }
                if (json_obj[key].is_number()) {
                    return std::to_string(json_obj[key].get<double>());
                }
                return default_value;
            };
            
            std::string order_id = safe_get_string(response_json, "id", "");
            std::string order_status = safe_get_string(response_json, "status", "");
            std::string filled_qty_value = safe_get_string(response_json, "filled_qty", "0");
            std::string filled_avg_price_value = safe_get_string(response_json, "filled_avg_price", "");
            std::string submitted_at_value = safe_get_string(response_json, "submitted_at", "");
            
            // Log order acceptance using table format
            TradingLogs::log_order_accepted("Stop-Limit Order", symbol_string, order_side_string, quantity_value,
                                             order_id, order_status, filled_qty_value, filled_avg_price_value,
                                             submitted_at_value, stop_price, limit_price);
            
            // Log full API response for debugging
            TradingLogs::log_api_response_full(api_response);
        } else {
            throw std::runtime_error("Unexpected API response format - no order ID or error code found. Response: " + api_response);
        }
    } catch (const std::runtime_error& runtime_exception_error) {
        throw;
    } catch (const std::exception& parse_exception_error) {
        throw std::runtime_error("Failed to parse API response: " + std::string(parse_exception_error.what()) + " | Raw response: " + api_response);
    }
}

// Crypto bracket order simulation - using separate orders + monitoring
// ALPACA DOCS: Bracket orders are NOT supported for crypto
// Solution: Place entry order, then separately place stop-loss and take-profit orders
// Monitoring will be handled by the position monitoring system
void OrderExecutionLogic::execute_crypto_bracket_simulation(OrderSide order_side_input, const ProcessedData& processed_data_input, const PositionSizing& position_sizing_input, const ExitTargets& exit_targets_input) {
    std::string order_side_string = (order_side_input == OrderSide::Buy) ? config.strategy.signal_buy_string : config.strategy.signal_sell_string;
    std::string symbol_string = config.trading_mode.primary_symbol;
    double stop_loss_price = exit_targets_input.stop_loss;
    double take_profit_price = exit_targets_input.take_profit;
    
    // Log crypto bracket simulation using table format
    TradingLogs::log_crypto_bracket_simulation(symbol_string, order_side_string, position_sizing_input.quantity,
                                               processed_data_input.curr.close_price, stop_loss_price, take_profit_price);
    
    // CRITICAL: Cancel existing orders before placing new ones to avoid wash trade detection
    // ALPACA DOCS: Wash trade detection occurs when opposite side orders exist
    // We must cancel any existing stop-loss or take-profit orders before placing a new entry order
    try {
        std::string open_orders_response = api_manager.get_open_orders();
        if (!open_orders_response.empty()) {
            try {
                json open_orders_json = json::parse(open_orders_response);
                
                // Check if response is an array of orders
                if (open_orders_json.is_array()) {
                    int cancelled_orders_count = 0;
                    for (const auto& order_obj : open_orders_json) {
                        // Extract order details safely
                        auto safe_get_string = [](const json& json_obj, const std::string& key, const std::string& default_value) -> std::string {
                            if (!json_obj.contains(key)) {
                                return default_value;
                            }
                            if (json_obj[key].is_null()) {
                                return default_value;
                            }
                            if (json_obj[key].is_string()) {
                                return json_obj[key].get<std::string>();
                            }
                            return default_value;
                        };
                        
                        std::string order_symbol = safe_get_string(order_obj, "symbol", "");
                        std::string order_id = safe_get_string(order_obj, "id", "");
                        std::string order_status = safe_get_string(order_obj, "status", "");
                        
                        // Cancel orders for this symbol that are not yet filled or cancelled
                        if (order_symbol == symbol_string && !order_id.empty()) {
                            // Attempt to cancel ALL orders that are not in final states
                            // Final states: filled, canceled, expired, rejected
                            // All other states should be attempted for cancellation (including held, partially_filled, etc.)
                            bool is_final_state = (order_status == "filled" || order_status == "canceled" || 
                                                   order_status == "expired" || order_status == "rejected");
                            
                            if (!is_final_state) {
                                try {
                                    api_manager.cancel_order(order_id);
                                    cancelled_orders_count++;
                                    std::ostringstream cancel_log_stream;
                                    cancel_log_stream << "Cancelled existing order to prevent wash trade - Order ID: " << order_id 
                                                      << " | Symbol: " << symbol_string 
                                                      << " | Status: " << order_status;
                                    log_message(cancel_log_stream.str(), "");
                                } catch (const std::exception& cancel_exception_error) {
                                    std::string error_message = std::string(cancel_exception_error.what());
                                    // If order is already filled/cancelled (404/not found), that's fine - log and continue
                                    // Otherwise, fail hard per compliance rules: system must work exactly as designed
                                    if (error_message.find("empty response") != std::string::npos || 
                                        error_message.find("404") != std::string::npos ||
                                        error_message.find("not found") != std::string::npos) {
                                        std::ostringstream already_gone_stream;
                                        already_gone_stream << "Order " << order_id << " already filled/cancelled (not found) - proceeding";
                                        log_message(already_gone_stream.str(), "");
                                    } else {
                                        // Real cancellation error - fail hard per compliance rules
                                        std::ostringstream cancel_error_stream;
                                        cancel_error_stream << "CRITICAL: Failed to cancel order " << order_id 
                                                           << " (Status: " << order_status 
                                                           << " | Symbol: " << symbol_string 
                                                           << ") - Error: " << error_message 
                                                           << " | System cannot proceed with wash trade prevention - failing hard";
                                        log_message(cancel_error_stream.str(), "");
                                        throw std::runtime_error("Order cancellation failed - cannot prevent wash trade: " + error_message);
                                    }
                                }
                            } else {
                                // Order already in final state - no need to cancel
                                std::ostringstream final_state_stream;
                                final_state_stream << "Order " << order_id << " already in final state (" << order_status << ") - skipping cancellation";
                                log_message(final_state_stream.str(), "");
                            }
                        }
                    }
                    
                    if (cancelled_orders_count > 0) {
                        std::ostringstream cancel_summary_stream;
                        cancel_summary_stream << "Cancelled " << cancelled_orders_count << " existing order(s) before placing new bracket simulation orders";
                        log_message(cancel_summary_stream.str(), "");
                        
                        // Wait for cancellation to process
                        int cancellation_delay_ms = config.timing.order_cancellation_processing_delay_milliseconds;
                        std::this_thread::sleep_for(std::chrono::milliseconds(cancellation_delay_ms));
                    }
                }
            } catch (const std::exception& parse_exception_error) {
                std::ostringstream parse_error_stream;
                parse_error_stream << "Failed to parse open orders response: " << std::string(parse_exception_error.what()) 
                                   << " | Response: " << open_orders_response;
                log_message(parse_error_stream.str(), "");
            }
        }
    } catch (const std::exception& orders_check_exception_error) {
        std::ostringstream check_error_stream;
        check_error_stream << "Failed to check/cancel existing orders: " << std::string(orders_check_exception_error.what()) 
                           << " | Proceeding with new order placement";
        log_message(check_error_stream.str(), "");
    }
    
    // Step 1: Place entry order (market order for immediate execution)
    try {
        execute_market_order(order_side_input, processed_data_input, position_sizing_input);
        
        // Step 2: Place stop-loss order (stop-limit order for risk management)
        // For buy orders: stop-loss below entry (sell stop-limit)
        // For sell orders: stop-loss above entry (buy stop-limit) - but crypto can't short, so this shouldn't happen
        if (order_side_input == OrderSide::Buy) {
            // Long position: stop-loss is a sell stop-limit order
            OrderSide stop_side = OrderSide::Sell;
            execute_stop_limit_order(stop_side, processed_data_input, position_sizing_input, stop_loss_price, stop_loss_price);
        } else {
            // Sell order for crypto - should only be closing existing position
            // ALPACA DOCS: Crypto cannot be sold short, so this path should only execute when closing long positions
            // For closing positions, stop-loss protection is handled by the position management system
            std::ostringstream sell_order_note_stream;
            sell_order_note_stream << "Sell order for crypto detected - This should only occur when closing existing long positions. "
                                   << "Stop-loss protection for closing positions is managed separately by position monitoring system.";
            log_message(sell_order_note_stream.str(), "");
        }
        
        // Step 3: Place take-profit order (limit order)
        // For buy orders: take-profit is a sell limit order
        // For sell orders: take-profit is a buy limit order
        OrderSide tp_side = (order_side_input == OrderSide::Buy) ? OrderSide::Sell : OrderSide::Buy;
        execute_limit_order(tp_side, processed_data_input, position_sizing_input, take_profit_price);
        
        TradingLogs::log_crypto_bracket_complete();
        
    } catch (const std::exception& exception_error) {
        std::ostringstream error_stream;
        error_stream << "CRYPTO BRACKET SIMULATION FAILED: " << std::string(exception_error.what()) 
                     << " | You may need to manually cancel any partial orders placed";
        log_message(error_stream.str(), "");
        throw std::runtime_error(error_stream.str());
    }
}

bool OrderExecutionLogic::validate_trade_feasibility(const PositionSizing& position_sizing_input, double buying_power_amount, double current_price_amount) const {
    if (position_sizing_input.quantity <= 0.0) {
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

    // CRITICAL DEFENSE: Validate position quantity before market close handling
    // Prevent execution with corrupted position data
    if (std::abs(current_position_quantity) > config.strategy.maximum_reasonable_position_quantity) {
        // Log the corruption but don't attempt to close invalid positions
        std::cerr << "CRITICAL: Detected corrupted position quantity (" << current_position_quantity
                  << ") in market close handling - skipping to prevent invalid orders" << std::endl;
        return false;
    }

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
