#include "order_client.hpp"
#include "../../logging/async_logger.hpp"
#include "../../logging/logging_macros.hpp"
#include "../../logging/trading_logger.hpp"
#include "../../utils/http_utils.hpp"
#include "../../json/json.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

// Helper function to round price to valid penny increments
std::string round_price_to_penny(double price) {
    // Round to 2 decimal places (penny increments)
    double rounded = std::round(price * 100.0) / 100.0;
    
    // Format with exactly 2 decimal places
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << rounded;
    return oss.str();
}

void OrderClient::place_bracket_order(const OrderRequest& oreq) const {
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

void OrderClient::close_position(const ClosePositionRequest& creq) const {
    if (creq.currentQty == 0) return;
    
    std::string side = (creq.currentQty > 0) ? "sell" : "buy";
    int abs_qty = std::abs(creq.currentQty);
    
    json order = json::object();
    order["symbol"] = target.symbol;
    order["qty"] = std::to_string(abs_qty);
    order["side"] = side;
    order["type"] = "market";
    order["time_in_force"] = "gtc";
    
    HttpRequest req(api.base_url + "/v2/orders", api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms, order.dump());
    std::string response = http_post(req);
    
    std::string log_msg = format_order_log("Closing position", side + " " + std::to_string(abs_qty));
    log_order_result(log_msg, response);
}

void OrderClient::log_order_result(const std::string& operation, const std::string& response) const {
    TradingLogger::log_order_result_table(operation, response);
}

std::string OrderClient::format_order_log(const std::string& operation, const std::string& details) const {
    return operation + ": " + details;
}
