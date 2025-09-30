
#include "api/orders/order_client.hpp"
#include "api/alpaca_client.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/logging_macros.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/utils/http_utils.hpp"
#include "json/json.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <string>

using json = nlohmann::json;

namespace AlpacaTrader {
namespace API {
namespace Orders {

// Using declarations for cleaner code
using AlpacaTrader::Logging::TradingLogs;
using AlpacaTrader::Logging::log_message;

// Helper function to round price to valid penny increments.
std::string round_price_to_penny(double price) {
    // Round to 2 decimal places (penny increments).
    double rounded = std::round(price * 100.0) / 100.0;
    
    // Format with exactly 2 decimal places.
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << rounded;
    return oss.str();
}

void OrderClient::set_api_client(void* client) {
    api_client = client;
}

// Helper method to get the API client with proper casting
void* OrderClient::get_api_client() const {
    return api_client;
}

void OrderClient::cancel_orders_for_signal(const std::string& signal_side) const {
    cancel_orders(signal_side);
}

void OrderClient::log_order_details_table(const nlohmann::json& order, const std::string& response) const {
    // Order Details Table
    TABLE_HEADER_48("ORDER DETAILS", "Bracket Order Configuration");
    
    TABLE_ROW_48("Symbol", order.value("symbol", "N/A"));
    TABLE_ROW_48("Side", order.value("side", "N/A"));
    TABLE_ROW_48("Quantity", order.value("qty", "N/A"));
    TABLE_ROW_48("Type", order.value("type", "N/A"));
    TABLE_ROW_48("Time in Force", order.value("time_in_force", "N/A"));
    TABLE_ROW_48("Order Class", order.value("order_class", "N/A"));
    
    if (order.contains("take_profit") && order["take_profit"].contains("limit_price")) {
        TABLE_ROW_48("Take Profit", "$" + order["take_profit"]["limit_price"].get<std::string>());
    }
    
    if (order.contains("stop_loss") && order["stop_loss"].contains("stop_price")) {
        TABLE_ROW_48("Stop Loss", "$" + order["stop_loss"]["stop_price"].get<std::string>());
    }
    
    TABLE_FOOTER_48();
    
    // Parse API Response
    try {
        nlohmann::json response_json = nlohmann::json::parse(response);
        
        TABLE_HEADER_48("API RESPONSE", "Order Execution Result");
        
        TABLE_ROW_48("Order ID", response_json.value("id", "N/A"));
        TABLE_ROW_48("Status", response_json.value("status", "N/A"));
        TABLE_ROW_48("Side", response_json.value("side", "N/A"));
        TABLE_ROW_48("Quantity", response_json.value("qty", "N/A"));
        TABLE_ROW_48("Order Class", response_json.value("order_class", "N/A"));
        TABLE_ROW_48("Position Intent", response_json.value("position_intent", "N/A"));
        
        if (response_json.contains("created_at")) {
            TABLE_ROW_48("Created At", response_json["created_at"].get<std::string>());
        }
        
        if (response_json.contains("filled_at") && !response_json["filled_at"].is_null()) {
            TABLE_ROW_48("Filled At", response_json["filled_at"].get<std::string>());
        } else {
            TABLE_ROW_48("Filled At", "Not filled");
        }
        
        if (response_json.contains("legs") && response_json["legs"].is_array()) {
            TABLE_SEPARATOR_48();
            TABLE_ROW_48("Bracket Legs", std::to_string(response_json["legs"].size()) + " legs");
            
            for (size_t i = 0; i < response_json["legs"].size(); ++i) {
                const auto& leg = response_json["legs"][i];
                std::string leg_type = leg.value("type", "unknown");
                std::string leg_side = leg.value("side", "unknown");
                std::string leg_price = "N/A";
                
                if (leg.contains("limit_price") && !leg["limit_price"].is_null()) {
                    leg_price = "$" + leg["limit_price"].get<std::string>();
                } else if (leg.contains("stop_price") && !leg["stop_price"].is_null()) {
                    leg_price = "$" + leg["stop_price"].get<std::string>();
                }
                
                TABLE_ROW_48("Leg " + std::to_string(i+1), leg_type + " " + leg_side + " @ " + leg_price);
            }
        }
        
        TABLE_FOOTER_48();
        
    } catch (const nlohmann::json::exception& e) {
        LOG_THREAD_CONTENT("API Response Parse Error: " + std::string(e.what()));
        LOG_THREAD_CONTENT("Raw Response: " + response);
    }
    
    // If response is empty or parsing failed, show raw response
    if (response.empty()) {
        LOG_THREAD_CONTENT("API Response: Empty response received");
    } else if (response.length() < 10) {
        LOG_THREAD_CONTENT("API Response: " + response);
    }
}

void OrderClient::place_bracket_order(const Core::OrderRequest& oreq) const {
    if (oreq.qty <= 0) return;
    
    json order = json::object();
    order["symbol"] = target.symbol;
    order["qty"] = std::to_string(oreq.qty);
    order["side"] = oreq.side;
    order["type"] = "market";
    order["time_in_force"] = "gtc";
    order["order_class"] = "bracket";
    
    json take_profit = json::object();
    json stop_loss = json::object();
    take_profit["limit_price"] = round_price_to_penny(oreq.tp);
    stop_loss["stop_price"] = round_price_to_penny(oreq.sl);
    
    order["take_profit"] = take_profit;
    order["stop_loss"] = stop_loss;
    
    HttpRequest req(api.base_url + "/v2/orders", api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms, order.dump());
    std::string response = http_post(req);
    
    // Log order details for troubleshooting
    LOG_THREAD_SECTION_HEADER("ORDER EXECUTION DEBUG");
    log_order_details_table(order, response);
    LOG_THREAD_SECTION_FOOTER();
    
    std::string log_msg = format_order_log("Bracket order attempt", 
        oreq.side + " " + std::to_string(oreq.qty) + " (TP: " + round_price_to_penny(oreq.tp) + 
        ", SL: " + round_price_to_penny(oreq.sl) + ")");
    
    log_order_result(log_msg, response);
}

void OrderClient::close_position(const Core::ClosePositionRequest& creq) const {
    if (creq.current_qty == 0) {
        TradingLogs::log_position_already_closed();
        return;
    }
    
    TradingLogs::log_position_closure_start(creq.current_qty);
    
    // Step 1: Cancel orders and wait
    cancel_orders_and_wait();
    
    // Step 2: Get fresh position data
    int actual_qty = get_fresh_position_quantity();
    if (actual_qty == 0) {
        TradingLogs::log_position_already_closed();
        return;
    }
    
    // Step 3: Submit closure order
    submit_position_closure_order(actual_qty);
    
    // Step 4: Verify closure
    verify_position_closure();
}

void OrderClient::log_order_result(const std::string& operation, const std::string& response) const {
    TradingLogs::log_order_result_table(operation, response);
}

std::string OrderClient::format_order_log(const std::string& operation, const std::string& details) const {
    return operation + ": " + details;
}

void OrderClient::cancel_orders(const std::string& new_signal_side) const {
    TradingLogs::log_cancellation_start(orders.cancellation_mode, new_signal_side);
    
    // Step 1: Fetch all open orders
    std::vector<std::string> all_order_ids = fetch_open_orders();
    if (all_order_ids.empty()) {
        TradingLogs::log_no_orders_to_cancel();
        return;
    }
    
    // Step 2: Filter orders based on cancellation strategy
    std::vector<std::string> orders_to_cancel = filter_orders_for_cancellation(all_order_ids, new_signal_side);
    
    // Step 3: Execute cancellations
    if (!orders_to_cancel.empty()) {
        execute_cancellations(orders_to_cancel);
    } else {
        TradingLogs::log_no_orders_to_cancel();
    }
}

std::string OrderClient::get_positions() const {
    void* client_ptr = get_api_client();
    if (!client_ptr) {
        log_message("ERROR: get_positions - API client not set", logging.log_file);
        return "[]";
    }
    
    auto* client = static_cast<AlpacaTrader::API::AlpacaClient*>(client_ptr);
    return client->get_positions();
}

int OrderClient::parse_position_quantity(const std::string& positions_response) const {
    try {
        json positions = json::parse(positions_response);
        if (positions.is_array()) {
            for (const auto& position : positions) {
                if (position.contains("symbol") && position["symbol"] == target.symbol) {
                    if (position.contains("qty")) {
                        std::string qty_str = position["qty"].get<std::string>();
                        int qty = std::stoi(qty_str);
                        return qty;
                    }
                }
            }
        } else {
        }
    } catch (const std::exception& e) {
    }
    return 0;
}

std::vector<std::string> OrderClient::fetch_open_orders() const {
    void* client_ptr = get_api_client();
    if (!client_ptr) {
        log_message("ERROR: fetch_open_orders - API client not set", logging.log_file);
        return {};
    }
    
    auto* client = static_cast<AlpacaTrader::API::AlpacaClient*>(client_ptr);
    std::vector<std::string> order_ids = client->get_open_orders(target.symbol);
    TradingLogs::log_orders_found(order_ids.size(), target.symbol);
    return order_ids;
}

std::vector<std::string> OrderClient::filter_orders_for_cancellation(const std::vector<std::string>& order_ids, const std::string& signal_side) const {
    std::vector<std::string> filtered_orders;
    std::string cancellation_reason;
    
    if (orders.cancellation_mode == "all") {
        // Cancel all orders regardless of side
        filtered_orders = order_ids;
        cancellation_reason = "all orders";
    } else if (orders.cancellation_mode == "directional" && !signal_side.empty()) {
        // Smart directional cancellation based on new signal
        for (const auto& order_id : order_ids) {
            // For now, we'll need to fetch individual order details to check the side
            // This could be optimized by storing order details in the initial fetch
            // But for now, we'll implement the simple approach
            filtered_orders.push_back(order_id);
        }
        cancellation_reason = "directional (" + signal_side + " signal)";
    } else {
        // Default: cancel all orders
        filtered_orders = order_ids;
        cancellation_reason = "all orders (default)";
    }
    
    // Limit number of orders to cancel
    if (filtered_orders.size() > static_cast<size_t>(orders.max_orders_to_cancel)) {
        filtered_orders.resize(orders.max_orders_to_cancel);
        log_message("WARNING: Limited to " + std::to_string(orders.max_orders_to_cancel) + " orders due to max_orders_to_cancel setting", logging.log_file);
    }
    
    TradingLogs::log_orders_filtered(filtered_orders.size(), cancellation_reason);
    
    return filtered_orders;
}

void OrderClient::execute_cancellations(const std::vector<std::string>& order_ids) const {
    if (order_ids.empty()) {
        TradingLogs::log_no_orders_to_cancel();
        return;
    }
    
    void* client_ptr = get_api_client();
    if (!client_ptr) {
        log_message("ERROR: execute_cancellations - API client not set", logging.log_file);
        return;
    }
    
    auto* client = static_cast<AlpacaTrader::API::AlpacaClient*>(client_ptr);
    // Use the API client to cancel orders in batch
    client->cancel_orders_batch(order_ids);
    TradingLogs::log_cancellation_complete(order_ids.size(), target.symbol);
}

bool OrderClient::should_cancel_order(const std::string& order_side, const std::string& signal_side) const {
    if (orders.cancellation_mode == "all") {
        return true;
    }
    
    if (orders.cancellation_mode == "directional" && !signal_side.empty()) {
        // Cancel opposite side orders if configured
        if (orders.cancel_opposite_side && 
            ((signal_side == "buy" && order_side == "sell") || 
             (signal_side == "sell" && order_side == "buy"))) {
            return true;
        }
        
        // Cancel same side orders if configured
        if (orders.cancel_same_side && 
            ((signal_side == "buy" && order_side == "buy") || 
             (signal_side == "sell" && order_side == "sell"))) {
            return true;
        }
    }
    
    return false;
}

void OrderClient::cancel_orders_and_wait() const {
    cancel_orders();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(timing.order_cancellation_wait_ms));
}

int OrderClient::get_fresh_position_quantity() const {
    std::string positions_response = get_positions();
    int actual_qty = parse_position_quantity(positions_response);
    TradingLogs::log_fresh_position_data(actual_qty);
    return actual_qty;
}

void OrderClient::submit_position_closure_order(int quantity) const {
    void* client_ptr = get_api_client();
    if (!client_ptr) {
        log_message("ERROR: submit_position_closure_order - API client not set", logging.log_file);
        return;
    }
    
    auto* client = static_cast<AlpacaTrader::API::AlpacaClient*>(client_ptr);
    std::string side = (quantity > 0) ? orders.position_closure_side_sell : orders.position_closure_side_buy;
    int abs_qty = std::abs(quantity);
    
    TradingLogs::log_closure_order_submitted(side, abs_qty);
    
    // Use the API client to submit the market order
    client->submit_market_order(target.symbol, side, abs_qty);
    
    // Log the result
    std::string log_msg = format_order_log("Closing position", side + " " + std::to_string(abs_qty));
    log_order_result(log_msg, "Order submitted via API");
}

void OrderClient::verify_position_closure() const {
    std::this_thread::sleep_for(std::chrono::milliseconds(timing.position_verification_wait_ms));
    
    int verify_qty = get_fresh_position_quantity();
    TradingLogs::log_position_verification(verify_qty);
}

} // namespace Orders
} // namespace API
} // namespace AlpacaTrader
