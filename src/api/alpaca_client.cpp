#include "alpaca_client.hpp"
#include "utils/http_utils.hpp"
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
    
    HttpRequest req(config.api.base_url + config.orders.orders_endpoint, config.api.api_key, config.api.api_secret, config.logging.log_file, 
                   config.api.retry_count, config.api.timeout_seconds, config.api.enable_ssl_verification, config.api.rate_limit_delay_ms, order.dump());
    http_post(req);
}

} // namespace API
} // namespace AlpacaTrader
