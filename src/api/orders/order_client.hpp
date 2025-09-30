#ifndef ORDER_CLIENT_HPP
#define ORDER_CLIENT_HPP

#include <string>
#include <vector>
#include "api/base/alpaca_base_client.hpp"
#include "core/trader/data/data_structures.hpp"
#include "json/json.hpp"

namespace AlpacaTrader {
namespace API {
namespace Orders {

class OrderClient : public AlpacaBaseClient {
public:
    explicit OrderClient(const AlpacaClientConfig& cfg) : AlpacaBaseClient(cfg) {}
    
    // Set reference to parent AlpacaClient for API calls
    void set_api_client(void* client);
    
    // Helper method to get the API client with proper casting
    void* get_api_client() const;
    
    // Order management operations
    void place_bracket_order(const Core::OrderRequest& req) const;
    void close_position(const Core::ClosePositionRequest& req) const;
    void cancel_orders_for_signal(const std::string& signal_side) const;
    
private:
    // Helper methods for order operations
    void log_order_result(const std::string& operation, const std::string& response) const;
    void log_order_details_table(const nlohmann::json& order, const std::string& response) const;
    std::string format_order_log(const std::string& operation, const std::string& details) const;
    
    // Position management helpers
    void cancel_orders(const std::string& new_signal_side = "") const;
    std::string get_positions() const;
    int parse_position_quantity(const std::string& positions_response) const;
    
    // Order cancellation helpers
    std::vector<std::string> fetch_open_orders() const;
    std::vector<std::string> filter_orders_for_cancellation(const std::vector<std::string>& order_ids, const std::string& signal_side) const;
    void execute_cancellations(const std::vector<std::string>& order_ids) const;
    bool should_cancel_order(const std::string& order_side, const std::string& signal_side) const;
    
    // Position closure helpers
    void cancel_orders_and_wait() const;
    int get_fresh_position_quantity() const;
    void submit_position_closure_order(int quantity) const;
    void verify_position_closure() const;
    
    // Reference to parent AlpacaClient for API calls
    void* api_client = nullptr;
};

} // namespace Orders
} // namespace API
} // namespace AlpacaTrader

#endif // ORDER_CLIENT_HPP
