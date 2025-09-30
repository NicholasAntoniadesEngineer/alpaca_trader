#include "alpaca_client.hpp"
#include "core/utils/http_utils.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/logging_macros.hpp"
#include "json/json.hpp"
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

using json = nlohmann::json;

namespace AlpacaTrader {
namespace API {

// Order cancellation API methods
std::vector<std::string> AlpacaClient::get_open_orders(const std::string& symbol) const {
    HttpRequest get_orders_req(config.api.base_url + config.orders.orders_endpoint + "?status=" + config.orders.orders_status_filter, 
                              config.api.api_key, config.api.api_secret, config.logging.log_file, 
                              config.api.retry_count, config.api.timeout_seconds, config.api.enable_ssl_verification, 0, "");
    std::string orders_response = http_get(get_orders_req);
    
    std::vector<std::string> order_ids;
    
    try {
        json orders_json = json::parse(orders_response);
        if (orders_json.is_array()) {
            for (const auto& order : orders_json) {
                if (order.contains("id") && order.contains("symbol") && order["symbol"] == symbol) {
                    order_ids.push_back(order["id"]);
                }
            }
        }
    } catch (const std::exception& e) {
        // Log error if needed
    }
    
    return order_ids;
}

void AlpacaClient::cancel_order(const std::string& order_id) const {
    HttpRequest cancel_req(config.api.base_url + config.orders.orders_endpoint + "/" + order_id, 
                          config.api.api_key, config.api.api_secret, config.logging.log_file, 
                          config.api.retry_count, config.api.timeout_seconds, config.api.enable_ssl_verification, 0, "");
    http_delete(cancel_req);
}

void AlpacaClient::cancel_orders_batch(const std::vector<std::string>& order_ids) const {
    // Cancel orders in parallel with controlled concurrency
    const int MAX_CONCURRENT = std::min(10, static_cast<int>(order_ids.size()));
    std::atomic<int> cancelled_count{0};
    std::atomic<int> current_index{0};
    
    auto cancel_worker = [this, &order_ids, &cancelled_count, &current_index]() {
        while (true) {
            int index = current_index.fetch_add(1);
            if (index >= static_cast<int>(order_ids.size())) break;
            
            const std::string& order_id = order_ids[index];
            try {
                cancel_order(order_id);
                cancelled_count++;
            } catch (const std::exception& e) {
                // Log error if needed
            }
        }
    };
    
    // Create worker threads
    std::vector<std::thread> workers;
    for (int i = 0; i < MAX_CONCURRENT; ++i) {
        workers.emplace_back(cancel_worker);
    }
    
    // Wait for all workers to complete
    for (auto& worker : workers) {
        worker.join();
    }
}

// Position management API methods
std::string AlpacaClient::get_positions() const {
    HttpRequest req(config.api.base_url + "/v2/positions", config.api.api_key, config.api.api_secret, config.logging.log_file, 
                   config.api.retry_count, config.api.timeout_seconds, config.api.enable_ssl_verification, config.api.rate_limit_delay_ms, "");
    return http_get(req);
}

int AlpacaClient::get_position_quantity(const std::string& symbol) const {
    std::string positions_response = get_positions();
    
    try {
        json positions = json::parse(positions_response);
        if (positions.is_array()) {
            for (const auto& position : positions) {
                if (position.contains("symbol") && position["symbol"] == symbol) {
                    if (position.contains("qty")) {
                        std::string qty_str = position["qty"].get<std::string>();
                        return std::stoi(qty_str);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        // Log error if needed
    }
    
    return 0;
}

void AlpacaClient::submit_market_order(const std::string& symbol, const std::string& side, int quantity) const {
    json order = json::object();
    order["symbol"] = symbol;
    order["qty"] = std::to_string(quantity);
    order["side"] = side;
    order["type"] = config.orders.default_order_type;
    order["time_in_force"] = config.orders.default_time_in_force;
    
    // Workaround for memory corruption: Use hardcoded values if config is corrupted
    std::string base_url = config.api.base_url;
    std::string orders_endpoint = config.orders.orders_endpoint;
    
    // Check if config values are corrupted (contain error messages)
    if (base_url.find("Unknown fatal error") != std::string::npos || 
        base_url.find("run_until_shutdown") != std::string::npos ||
        orders_endpoint.empty()) {
        // Use correct hardcoded values
        base_url = "https://paper-api.alpaca.markets";
        orders_endpoint = "/v2/orders";
        AlpacaTrader::Logging::log_message("WARNING: Config corrupted, using hardcoded values", config.logging.log_file);
    }
    
    std::string full_url = base_url + orders_endpoint;
    
    // Log order submission details in table format
    AlpacaTrader::Logging::log_message("+-- ORDER SUBMISSION", config.logging.log_file);
    AlpacaTrader::Logging::log_message("|   URL: " + full_url, config.logging.log_file);
    AlpacaTrader::Logging::log_message("|   Symbol: " + symbol, config.logging.log_file);
    AlpacaTrader::Logging::log_message("|   Side: " + side, config.logging.log_file);
    AlpacaTrader::Logging::log_message("|   Quantity: " + std::to_string(quantity), config.logging.log_file);
    AlpacaTrader::Logging::log_message("|   Type: " + std::string(order["type"]), config.logging.log_file);
    AlpacaTrader::Logging::log_message("|   Time in Force: " + std::string(order["time_in_force"]), config.logging.log_file);
    AlpacaTrader::Logging::log_message("+-- ", config.logging.log_file);
    
    HttpRequest req(full_url, config.api.api_key, config.api.api_secret, config.logging.log_file, 
                   config.api.retry_count, config.api.timeout_seconds, config.api.enable_ssl_verification, config.api.rate_limit_delay_ms, order.dump());
    
    std::string response = http_post(req);
    
    // Parse and log API response in table format
    try {
        json response_json = json::parse(response);
        
        AlpacaTrader::Logging::log_message("+-- API RESPONSE", config.logging.log_file);
        AlpacaTrader::Logging::log_message("|   Order ID: " + response_json.value("id", "N/A"), config.logging.log_file);
        AlpacaTrader::Logging::log_message("|   Status: " + response_json.value("status", "N/A"), config.logging.log_file);
        AlpacaTrader::Logging::log_message("|   Side: " + response_json.value("side", "N/A"), config.logging.log_file);
        AlpacaTrader::Logging::log_message("|   Quantity: " + response_json.value("qty", "N/A"), config.logging.log_file);
        AlpacaTrader::Logging::log_message("|   Position Intent: " + response_json.value("position_intent", "N/A"), config.logging.log_file);
        
        if (response_json.contains("created_at")) {
            AlpacaTrader::Logging::log_message("|   Created At: " + response_json["created_at"].get<std::string>(), config.logging.log_file);
        }
        
        if (response_json.contains("filled_at") && !response_json["filled_at"].is_null()) {
            AlpacaTrader::Logging::log_message("|   Filled At: " + response_json["filled_at"].get<std::string>(), config.logging.log_file);
        } else {
            AlpacaTrader::Logging::log_message("|   Filled At: Not filled", config.logging.log_file);
        }
        
        AlpacaTrader::Logging::log_message("+-- ", config.logging.log_file);
        
    } catch (const json::exception& e) {
        AlpacaTrader::Logging::log_message("+-- API RESPONSE ERROR", config.logging.log_file);
        AlpacaTrader::Logging::log_message("|   Parse Error: " + std::string(e.what()), config.logging.log_file);
        AlpacaTrader::Logging::log_message("|   Raw Response: " + response, config.logging.log_file);
        AlpacaTrader::Logging::log_message("+-- ", config.logging.log_file);
    }
}

} // namespace API
} // namespace AlpacaTrader
