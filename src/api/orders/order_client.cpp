
#include "api/orders/order_client.hpp"
#include "api/alpaca_client.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/logging_macros.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/utils/http_utils.hpp"
#include "configs/api_config.hpp"
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
    // Parse API Response using consolidated logging
    try {
        nlohmann::json response_json = nlohmann::json::parse(response);
        
        // Check if this is an error response
        if (response_json.contains("code") && response_json.contains("message")) {
            // Extract error response data
            std::string error_code = std::to_string(response_json.value("code", 0));
            std::string error_message = response_json.value("message", "Unknown error");
            std::string symbol = response_json.value("symbol", "N/A");
            std::string requested_qty = order.value("qty", "N/A");
            std::string available_qty = response_json.value("available", "N/A");
            std::string existing_qty = response_json.value("existing_qty", "N/A");
            std::string held_for_strategy = response_json.value("held_for_strategy", "N/A");
            std::string related_strategy = "";
            if (response_json.contains("related_strategy") && response_json["related_strategy"].is_array() && !response_json["related_strategy"].empty()) {
                related_strategy = response_json["related_strategy"][0].get<std::string>();
            }
            
            // Use consolidated error logging
            AlpacaTrader::Logging::TradingLogs::log_comprehensive_api_response("", "", "", requested_qty, "", "", "", "", "", "",
                                                       error_code, error_message, available_qty, existing_qty, 
                                                       held_for_strategy, related_strategy);
        } else {
            // Extract success response data
            std::string order_id = response_json.value("id", "N/A");
            std::string status = response_json.value("status", "N/A");
            std::string side = response_json.value("side", "N/A");
            std::string qty = response_json.value("qty", "N/A");
            std::string order_class = response_json.value("order_class", "N/A");
            std::string position_intent = response_json.value("position_intent", "N/A");
            std::string created_at = response_json.value("created_at", "N/A");
            std::string filled_at = "Not filled";
            std::string filled_qty = "0";
            std::string filled_avg_price = "N/A";
            
            // Format timestamps
            if (created_at != "N/A" && created_at.length() > 19) {
                created_at = created_at.substr(0, 19); // Remove microseconds and timezone
            }
            
            if (response_json.contains("filled_at") && !response_json["filled_at"].is_null()) {
                filled_at = response_json["filled_at"].get<std::string>();
                if (filled_at.length() > 19) {
                    filled_at = filled_at.substr(0, 19);
                }
            }
            
            if (response_json.contains("filled_qty") && !response_json["filled_qty"].is_null()) {
                filled_qty = response_json["filled_qty"].get<std::string>();
            }
            
            if (response_json.contains("filled_avg_price") && !response_json["filled_avg_price"].is_null()) {
                filled_avg_price = response_json["filled_avg_price"].get<std::string>();
            }
            
            // Use consolidated success logging
            AlpacaTrader::Logging::TradingLogs::log_comprehensive_api_response(order_id, status, side, qty, order_class, position_intent,
                                                       created_at, filled_at, filled_qty, filled_avg_price);
        }
        
    } catch (const nlohmann::json::exception& e) {
        // Use consolidated error logging for parse errors
        AlpacaTrader::Logging::TradingLogs::log_comprehensive_api_response("", "", "", "", "", "", "", "", "", "",
                                                   "PARSE_ERROR", std::string(e.what()), "", "", "", response.substr(0, 50) + "...");
    }
}

void OrderClient::place_bracket_order(const Core::OrderRequest& oreq) const {
    if (oreq.qty <= 0) return;
    
    json order = json::object();
    order["symbol"] = strategy.symbol;
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
    
    using namespace AlpacaTrader::Config;
    HttpRequest req(api.base_url + api.endpoints.trading.orders, api.api_key, api.api_secret, logging.log_file, 
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
    
    // Step 1: Cancel strategy and wait
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
    TradingLogs::log_cancellation_start(strategy.cancellation_mode, new_signal_side);
    
    // Step 1: Fetch all open strategy
    std::vector<std::string> all_order_ids = fetch_open_orders();
    if (all_order_ids.empty()) {
        TradingLogs::log_no_orders_to_cancel();
        return;
    }
    
    // Step 2: Filter strategy based on cancellation strategy
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
                if (position.contains("symbol") && position["symbol"] == strategy.symbol) {
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
    std::vector<std::string> order_ids = client->get_open_orders(strategy.symbol);
    TradingLogs::log_orders_found(order_ids.size(), strategy.symbol);
    return order_ids;
}

std::vector<std::string> OrderClient::filter_orders_for_cancellation(const std::vector<std::string>& order_ids, const std::string& signal_side) const {
    std::vector<std::string> filtered_orders;
    std::string cancellation_reason;
    
    if (strategy.cancellation_mode == "all") {
        // Cancel all strategy regardless of side
        filtered_orders = order_ids;
        cancellation_reason = "all strategy";
    } else if (strategy.cancellation_mode == "directional" && !signal_side.empty()) {
        // Smart directional cancellation based on new signal
        for (const auto& order_id : order_ids) {
            // For now, we'll need to fetch individual order details to check the side
            // This could be optimized by storing order details in the initial fetch
            // But for now, we'll implement the simple approach
            filtered_orders.push_back(order_id);
        }
        cancellation_reason = "directional (" + signal_side + " signal)";
    } else {
        // Default: cancel all strategy
        filtered_orders = order_ids;
        cancellation_reason = "all strategy (default)";
    }
    
    // Limit number of strategy to cancel
    if (filtered_orders.size() > static_cast<size_t>(strategy.max_orders_to_cancel)) {
        filtered_orders.resize(strategy.max_orders_to_cancel);
        log_message("WARNING: Limited to " + std::to_string(strategy.max_orders_to_cancel) + " orders due to max_orders_to_cancel setting", logging.log_file);
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
    // Use the API client to cancel strategy in batch
    client->cancel_orders_batch(order_ids);
    TradingLogs::log_cancellation_complete(order_ids.size(), strategy.symbol);
}

bool OrderClient::should_cancel_order(const std::string& order_side, const std::string& signal_side) const {
    if (strategy.cancellation_mode == "all") {
        return true;
    }
    
    if (strategy.cancellation_mode == "directional" && !signal_side.empty()) {
        // Cancel opposite side strategy if configured
        if (strategy.cancel_opposite_side && 
            ((signal_side == "buy" && order_side == "sell") || 
             (signal_side == "sell" && order_side == "buy"))) {
            return true;
        }
        
        // Cancel same side strategy if configured
        if (strategy.cancel_same_side && 
            ((signal_side == "buy" && order_side == "buy") || 
             (signal_side == "sell" && order_side == "sell"))) {
            return true;
        }
    }
    
    return false;
}

void OrderClient::cancel_orders_and_wait() const {
    cancel_orders("");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(timing.order_cancellation_processing_delay_milliseconds));
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
    std::string side = (quantity > 0) ? strategy.position_closure_side_sell : strategy.position_closure_side_buy;
    int abs_qty = std::abs(quantity);
    
    TradingLogs::log_closure_order_submitted(side, abs_qty);
    
    // Use the API client to submit the market order
    client->submit_market_order(strategy.symbol, side, abs_qty);
    
    // Log the result
    std::string log_msg = format_order_log("Closing position", side + " " + std::to_string(abs_qty));
    log_order_result(log_msg, "Order submitted via API");
}

void OrderClient::verify_position_closure() const {
    std::this_thread::sleep_for(std::chrono::milliseconds(timing.position_verification_timeout_milliseconds));
    
    int verify_qty = get_fresh_position_quantity();
    TradingLogs::log_position_verification(verify_qty);
}

} // namespace Orders
} // namespace API
} // namespace AlpacaTrader
