#include "api/orders/order_client.hpp"
#include "api/alpaca_client.hpp"
#include "logging/async_logger.hpp"
#include "logging/logging_macros.hpp"
#include "logging/trading_logs.hpp"
#include "utils/http_utils.hpp"
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
