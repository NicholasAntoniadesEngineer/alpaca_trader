#ifndef ORDER_CLIENT_HPP
#define ORDER_CLIENT_HPP

#include "../base/alpaca_base_client.hpp"
#include "../../data/data_structures.hpp"

class OrderClient : public AlpacaBaseClient {
public:
    explicit OrderClient(const AlpacaClientConfig& cfg) : AlpacaBaseClient(cfg) {}
    
    // Order management operations
    void place_bracket_order(const OrderRequest& req) const;
    void close_position(const ClosePositionRequest& req) const;
    
private:
    // Helper methods for order operations
    void log_order_result(const std::string& operation, const std::string& response) const;
    std::string format_order_log(const std::string& operation, const std::string& details) const;
};

#endif // ORDER_CLIENT_HPP
