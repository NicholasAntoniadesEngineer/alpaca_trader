#include "alpaca_stocks_client.hpp"
#include "utils/http_utils.hpp"
#include "json/json.hpp"
#include <stdexcept>

using json = nlohmann::json;

namespace AlpacaTrader {
namespace API {

AlpacaStocksClient::AlpacaStocksClient(ConnectivityManager& connectivity_mgr) 
    : connected(false), connectivity_manager(connectivity_mgr) {}

AlpacaStocksClient::~AlpacaStocksClient() {
    disconnect();
}

bool AlpacaStocksClient::initialize(const Config::ApiProviderConfig& configuration) {
    if (!validate_config()) {
        return false;
    }
    
    config = configuration;
    
    if (config.api_key.empty()) {
        throw std::runtime_error("Alpaca stocks API key is required but not provided");
    }
    
    if (config.api_secret.empty()) {
        throw std::runtime_error("Alpaca stocks API secret is required but not provided");
    }
    
    if (config.base_url.empty()) {
        throw std::runtime_error("Alpaca stocks base URL is required but not provided");
    }
    
    connected = true;
    return true;
}

bool AlpacaStocksClient::is_connected() const {
    return connected;
}

void AlpacaStocksClient::disconnect() {
    connected = false;
}

std::vector<Core::Bar> AlpacaStocksClient::get_recent_bars(const Core::BarRequest& request) const {
    if (!is_connected()) {
        throw std::runtime_error("Alpaca stocks client not connected");
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
    
    std::string response = make_authenticated_request(request_url);
    
    std::vector<Core::Bar> bars;
    
    try {
        json response_json = json::parse(response);
        
        if (!response_json.contains("bars") || !response_json["bars"].is_array()) {
            throw std::runtime_error("Invalid response format from Alpaca stocks bars API");
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
        throw std::runtime_error("Failed to parse Alpaca stocks bars response: " + std::string(exception_error.what()));
    }
    
    return bars;
}

std::vector<Core::Bar> AlpacaStocksClient::get_historical_bars(const std::string& /*symbol*/, const std::string& /*timeframe*/,
                                                              const std::string& /*start_date*/, const std::string& /*end_date*/,
                                                              int /*limit*/) const {
    // Alpaca stocks API may not support the same historical data endpoints as Polygon
    // This is a placeholder implementation - would need to be implemented based on Alpaca's API
    throw std::runtime_error("Historical bars not implemented for Alpaca stocks API - use Polygon for historical data");
}

double AlpacaStocksClient::get_current_price(const std::string& symbol) const {
    if (!is_connected()) {
        throw std::runtime_error("Alpaca stocks client not connected");
    }
    
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for price request");
    }
    
    std::string request_url = build_url_with_symbol(config.endpoints.quotes_latest, symbol);
    std::string response = make_authenticated_request(request_url);
    
    try {
        json response_json = json::parse(response);
        
        if (!response_json.contains("quote") || !response_json["quote"].is_object()) {
            throw std::runtime_error("Invalid response format from Alpaca stocks quotes API");
        }
        
        const auto& quote_data = response_json["quote"];
        if (!quote_data.contains("ap") || !quote_data.contains("bp")) {
            throw std::runtime_error("Price data not found in Alpaca stocks response");
        }
        
        double ask_price_value = quote_data["ap"].get<double>();
        double bid_price_value = quote_data["bp"].get<double>();
        
        return (ask_price_value + bid_price_value) / 2.0;
        
    } catch (const std::exception& exception_error) {
        throw std::runtime_error("Failed to parse Alpaca stocks price response: " + std::string(exception_error.what()));
    }
}

Core::QuoteData AlpacaStocksClient::get_realtime_quotes(const std::string& symbol) const {
    if (!is_connected()) {
        throw std::runtime_error("Alpaca stocks client not connected");
    }
    
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for quote request");
    }
    
    std::string request_url = build_url_with_symbol(config.endpoints.quotes_latest, symbol);
    std::string response = make_authenticated_request(request_url);
    
    Core::QuoteData quote_data;
    
    try {
        json response_json = json::parse(response);
        
        if (!response_json.contains("quote") || !response_json["quote"].is_object()) {
            throw std::runtime_error("Invalid response format from Alpaca stocks quotes API");
        }
        
        const auto& quote_object = response_json["quote"];
        quote_data.ask_price = quote_object.value("ap", 0.0);
        quote_data.bid_price = quote_object.value("bp", 0.0);
        quote_data.ask_size = quote_object.value("as", 0.0);
        quote_data.bid_size = quote_object.value("bs", 0.0);
        quote_data.timestamp = quote_object.value("t", "");
        quote_data.mid_price = (quote_data.ask_price + quote_data.bid_price) / 2.0;
        
    } catch (const std::exception& exception_error) {
        throw std::runtime_error("Failed to parse Alpaca stocks quote response: " + std::string(exception_error.what()));
    }
    
    return quote_data;
}

bool AlpacaStocksClient::is_market_open() const {
    return true;
}

bool AlpacaStocksClient::is_within_trading_hours() const {
    return true;
}

std::string AlpacaStocksClient::get_provider_name() const {
    return "Alpaca Stocks";
}

Config::ApiProvider AlpacaStocksClient::get_provider_type() const {
    return Config::ApiProvider::ALPACA_STOCKS;
}

std::string AlpacaStocksClient::make_authenticated_request(const std::string& request_url) const {
    if (request_url.empty()) {
        throw std::runtime_error("URL is required for authenticated request");
    }
    
    try {
        HttpRequest http_request(request_url, config.api_key, config.api_secret, config.retry_count, 
                           config.timeout_seconds, config.enable_ssl_verification, 
                           config.rate_limit_delay_ms, "");
        
        std::string response = http_get(http_request, connectivity_manager);
        
        if (response.empty()) {
            throw std::runtime_error("Empty response from Alpaca stocks API");
        }
        
        return response;
    } catch (const std::exception& exception_error) {
        throw std::runtime_error("Alpaca stocks API request failed: " + std::string(exception_error.what()) + " for URL: " + request_url);
    }
}

std::string AlpacaStocksClient::build_url_with_symbol(const std::string& endpoint, const std::string& symbol) const {
    if (endpoint.empty()) {
        throw std::runtime_error("Endpoint is required for URL construction");
    }
    
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for URL construction");
    }
    
    std::string constructed_url = config.base_url + endpoint;
    
    return replace_url_placeholder(constructed_url, symbol);
}

bool AlpacaStocksClient::validate_config() const {
    return true;
}

} // namespace API
} // namespace AlpacaTrader
