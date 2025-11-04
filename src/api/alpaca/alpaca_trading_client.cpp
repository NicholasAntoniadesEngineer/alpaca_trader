#include "alpaca_trading_client.hpp"
#include "utils/http_utils.hpp"
#include "json/json.hpp"
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace AlpacaTrader {
namespace API {

AlpacaTradingClient::AlpacaTradingClient(ConnectivityManager& connectivity_mgr) 
    : connected(false), connectivity_manager(connectivity_mgr) {}

AlpacaTradingClient::~AlpacaTradingClient() {
    disconnect();
}

bool AlpacaTradingClient::initialize(const Config::ApiProviderConfig& cfg) {
    if (!validate_config()) {
        return false;
    }
    
    config = cfg;
    
    if (config.api_key.empty()) {
        throw std::runtime_error("Alpaca API key is required but not provided");
    }
    
    if (config.api_secret.empty()) {
        throw std::runtime_error("Alpaca API secret is required but not provided");
    }
    
    if (config.base_url.empty()) {
        throw std::runtime_error("Alpaca base URL is required but not provided");
    }
    
    connected = true;
    return true;
}

bool AlpacaTradingClient::is_connected() const {
    return connected;
}

void AlpacaTradingClient::disconnect() {
    connected = false;
}

std::vector<Core::Bar> AlpacaTradingClient::get_recent_bars(const Core::BarRequest& request) const {
    if (!is_connected()) {
        throw std::runtime_error("Alpaca trading client not connected");
    }
    
    if (request.symbol.empty()) {
        throw std::runtime_error("Symbol is required for bar request");
    }
    
    if (request.limit <= 0) {
        throw std::runtime_error("Limit must be greater than 0 for bar request");
    }
    
    std::string request_url = build_url_with_symbol(config.endpoints.bars, request.symbol);
    request_url += "?limit=" + std::to_string(request.limit);
    request_url += "&timeframe=1Min";
    
    std::string response = make_authenticated_request(request_url, "GET", "");
    
    std::vector<Core::Bar> bars;
    
    try {
        json response_json = json::parse(response);
        
        if (!response_json.contains("bars") || !response_json["bars"].is_array()) {
            throw std::runtime_error("Invalid response format from Alpaca bars API");
        }
        
        for (const auto& bar_data : response_json["bars"]) {
            if (!bar_data.contains("o") || !bar_data.contains("h") || 
                !bar_data.contains("l") || !bar_data.contains("c") ||
                !bar_data.contains("v") || !bar_data.contains("t")) {
                continue;
            }
            
            Core::Bar bar;
            bar.open_price = bar_data["o"].get<double>();
            bar.high_price = bar_data["h"].get<double>();
            bar.low_price = bar_data["l"].get<double>();
            bar.close_price = bar_data["c"].get<double>();
            bar.volume = bar_data["v"].get<double>();
            bar.timestamp = bar_data["t"].get<std::string>();
            
            bars.push_back(bar);
        }
        
    } catch (const std::exception& exception_error) {
        throw std::runtime_error("Failed to parse Alpaca bars response: " + std::string(exception_error.what()));
    }
    
    return bars;
}

double AlpacaTradingClient::get_current_price(const std::string& symbol) const {
    if (!is_connected()) {
        throw std::runtime_error("Alpaca trading client not connected");
    }
    
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for price request");
    }
    
    std::string request_url = build_url_with_symbol(config.endpoints.quotes_latest, symbol);
    std::string response = make_authenticated_request(request_url, "GET", "");
    
    try {
        json response_json = json::parse(response);
        
        if (!response_json.contains("quote") || !response_json["quote"].is_object()) {
            throw std::runtime_error("Invalid response format from Alpaca quotes API");
        }
        
        const auto& quote_data = response_json["quote"];
        if (!quote_data.contains("ap") || !quote_data.contains("bp")) {
            throw std::runtime_error("Price data not found in Alpaca response");
        }
        
        double ask_price_value = quote_data["ap"].get<double>();
        double bid_price_value = quote_data["bp"].get<double>();
        
        return (ask_price_value + bid_price_value) / 2.0;
        
    } catch (const std::exception& exception_error) {
        throw std::runtime_error("Failed to parse Alpaca price response: " + std::string(exception_error.what()));
    }
}

Core::QuoteData AlpacaTradingClient::get_realtime_quotes(const std::string& symbol) const {
    if (!is_connected()) {
        throw std::runtime_error("Alpaca trading client not connected");
    }
    
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for quote request");
    }
    
    std::string request_url = build_url_with_symbol(config.endpoints.quotes_latest, symbol);
    std::string response = make_authenticated_request(request_url, "GET", "");
    
    Core::QuoteData quote_data;
    
    try {
        json response_json = json::parse(response);
        
        if (!response_json.contains("quote") || !response_json["quote"].is_object()) {
            throw std::runtime_error("Invalid response format from Alpaca quotes API");
        }
        
        const auto& quote_object = response_json["quote"];
        quote_data.ask_price = quote_object.value("ap", 0.0);
        quote_data.bid_price = quote_object.value("bp", 0.0);
        quote_data.ask_size = quote_object.value("as", 0.0);
        quote_data.bid_size = quote_object.value("bs", 0.0);
        quote_data.timestamp = quote_object.value("t", "");
        quote_data.mid_price = (quote_data.ask_price + quote_data.bid_price) / 2.0;
        
    } catch (const std::exception& exception_error) {
        throw std::runtime_error("Failed to parse Alpaca quote response: " + std::string(exception_error.what()));
    }
    
    return quote_data;
}

bool AlpacaTradingClient::is_market_open() const {
    if (!is_connected()) {
        return false;
    }
    
    std::string request_url = build_url(config.endpoints.clock);
    std::string response = make_authenticated_request(request_url, "GET", "");
    
    if (response.empty()) {
        throw std::runtime_error("Empty response from market open check API");
    }
    
    json response_json = json::parse(response);
    if (!response_json.contains("is_open")) {
        throw std::runtime_error("Invalid response format from market open check API - missing is_open field");
    }
    
    return response_json.value("is_open", false);
}

bool AlpacaTradingClient::is_within_trading_hours() const {
    return is_market_open();
}

std::string AlpacaTradingClient::get_provider_name() const {
    return "Alpaca Trading";
}

Config::ApiProvider AlpacaTradingClient::get_provider_type() const {
    return Config::ApiProvider::ALPACA_TRADING;
}

std::string AlpacaTradingClient::get_account_info() const {
    if (!is_connected()) {
        throw std::runtime_error("Alpaca trading client not connected");
    }
    
    std::string request_url = build_url(config.endpoints.account);
    return make_authenticated_request(request_url, "GET", "");
}

std::string AlpacaTradingClient::get_positions() const {
    if (!is_connected()) {
        throw std::runtime_error("Alpaca trading client not connected");
    }
    
    std::string request_url = build_url(config.endpoints.positions);
    return make_authenticated_request(request_url, "GET", "");
}

std::string AlpacaTradingClient::get_open_orders() const {
    if (!is_connected()) {
        throw std::runtime_error("Alpaca trading client not connected");
    }
    
    std::string request_url = build_url(config.endpoints.orders) + "?status=open";
    return make_authenticated_request(request_url, "GET", "");
}

void AlpacaTradingClient::place_order(const std::string& order_json) const {
    if (!is_connected()) {
        throw std::runtime_error("Alpaca trading client not connected");
    }
    
    if (order_json.empty()) {
        throw std::runtime_error("Order JSON is required");
    }
    
    std::string request_url = build_url(config.endpoints.orders);
    make_authenticated_request(request_url, "POST", order_json);
}

void AlpacaTradingClient::cancel_order(const std::string& order_id) const {
    if (!is_connected()) {
        throw std::runtime_error("Alpaca trading client not connected");
    }
    
    if (order_id.empty()) {
        throw std::runtime_error("Order ID is required");
    }
    
    std::string request_url = build_url(config.endpoints.orders) + "/" + order_id;
    make_authenticated_request(request_url, "DELETE", "");
}

void AlpacaTradingClient::close_position(const std::string& symbol, int quantity) const {
    if (!is_connected()) {
        throw std::runtime_error("Alpaca trading client not connected");
    }
    
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for position closure");
    }
    
    if (quantity == 0) {
        throw std::runtime_error("Quantity must be non-zero for position closure");
    }
    
    std::string request_url = build_url(config.endpoints.positions) + "/" + symbol;
    
    json close_request;
    close_request["qty"] = std::to_string(abs(quantity));
    
    make_authenticated_request(request_url, "DELETE", close_request.dump());
}

std::string AlpacaTradingClient::make_authenticated_request(const std::string& request_url, const std::string& method, 
                                                          const std::string& body) const {
    if (request_url.empty()) {
        throw std::runtime_error("URL is required for authenticated request");
    }
    
    // Validate configuration before making request
    if (config.api_key.empty()) {
        throw std::runtime_error("Alpaca API key is not configured");
    }
    
    if (config.api_secret.empty()) {
        throw std::runtime_error("Alpaca API secret is not configured");
    }
    
    if (config.base_url.empty()) {
        throw std::runtime_error("Alpaca base URL is not configured");
    }
    
    HttpRequest http_request(request_url, config.api_key, config.api_secret, config.retry_count, 
                       config.timeout_seconds, config.enable_ssl_verification, 
                       config.rate_limit_delay_ms, body);
    
    std::string response;
    std::string error_context = "Alpaca API " + method + " request to " + request_url;
    
    try {
        if (method == "GET") {
            response = http_get(http_request, connectivity_manager);
        } else if (method == "POST") {
            response = http_post(http_request, connectivity_manager);
        } else if (method == "DELETE") {
            response = http_delete(http_request, connectivity_manager);
        } else {
            throw std::runtime_error("Unsupported HTTP method: " + method);
        }
        
        if (response.empty()) {
            // Provide detailed error information for debugging
            std::string detailed_error = error_context + " returned empty response. ";
            detailed_error += "This could indicate: ";
            detailed_error += "1) Invalid API credentials (check api_key and api_secret), ";
            detailed_error += "2) Network connectivity issues, ";
            detailed_error += "3) API endpoint not accessible, ";
            detailed_error += "4) Rate limiting or API blocking, ";
            detailed_error += "5) SSL/TLS certificate issues. ";
            detailed_error += "Base URL: " + config.base_url + ", ";
            detailed_error += "API Key: " + config.api_key.substr(0, 8) + "...";
            throw std::runtime_error(detailed_error);
        }
        
        return response;
        
    } catch (const std::exception& exception_error) {
        // Re-throw with additional context
        std::string enhanced_error = error_context + " failed: " + std::string(exception_error.what());
        throw std::runtime_error(enhanced_error);
    }
}

std::string AlpacaTradingClient::build_url(const std::string& endpoint) const {
    if (endpoint.empty()) {
        throw std::runtime_error("Endpoint is required for URL construction");
    }
    
    return config.base_url + endpoint;
}

std::string AlpacaTradingClient::build_url_with_symbol(const std::string& endpoint, const std::string& symbol) const {
    if (endpoint.empty()) {
        throw std::runtime_error("Endpoint is required for URL construction");
    }
    
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for URL construction");
    }
    
    std::string constructed_url = config.base_url + endpoint;
    
    return replace_url_placeholder(constructed_url, symbol);
}

bool AlpacaTradingClient::validate_config() const {
    return true;
}

} // namespace API
} // namespace AlpacaTrader
