#include "polygon_crypto_client.hpp"
#include "core/logging/logger/async_logger.hpp"
#include "core/logging/logger/logging_macros.hpp"
#include "core/utils/http_utils.hpp"
#include "json/json.hpp"
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <stdexcept>

using json = nlohmann::json;

namespace AlpacaTrader {
namespace API {

PolygonCryptoClient::PolygonCryptoClient() = default;

PolygonCryptoClient::~PolygonCryptoClient() {
    disconnect();
}

bool PolygonCryptoClient::initialize(const Config::ApiProviderConfig& cfg) {
    if (!validate_config()) {
        return false;
    }
    
    config = cfg;
    
    if (config.api_key.empty()) {
        throw std::runtime_error("Polygon.io API key is required but not provided");
    }
    
    if (config.base_url.empty()) {
        throw std::runtime_error("Polygon.io base URL is required but not provided");
    }
    
    connected = true;
    return true;
}

bool PolygonCryptoClient::is_connected() const {
    return connected.load();
}

void PolygonCryptoClient::disconnect() {
    connected = false;
    stop_realtime_feed();
    cleanup_resources();
}

std::vector<Core::Bar> PolygonCryptoClient::get_recent_bars(const Core::BarRequest& request) const {
    if (!is_connected()) {
        throw std::runtime_error("Polygon client not connected");
    }
    
    if (request.symbol.empty()) {
        throw std::runtime_error("Symbol is required for bar request");
    }
    
    if (request.limit <= 0) {
        throw std::runtime_error("Limit must be greater than 0 for bar request");
    }

    if (config.bar_multiplier <= 0) {
        throw std::runtime_error("Polygon bar_multiplier must be configured and > 0");
    }
    if (config.bar_timespan.empty()) {
        throw std::runtime_error("Polygon bar_timespan must be configured (e.g., minute, hour, day)");
    }
    
    std::string url = build_rest_url(config.endpoints.bars, request.symbol);
    url += "&limit=" + std::to_string(request.limit);
    url += "&timespan=" + config.bar_timespan + "&multiplier=" + std::to_string(config.bar_multiplier);
    
    std::string response = make_authenticated_request(url);
    
    if (response.empty() || response[0] != '{') {
        throw std::runtime_error("Polygon bars response is not JSON (possible HTTP error): " + (response.size() > 64 ? response.substr(0, 64) : response));
    }
    
    std::vector<Core::Bar> bars;
    
    try {
        json response_json = json::parse(response);
        
        if (!response_json.contains("results") || !response_json["results"].is_array()) {
            throw std::runtime_error("Invalid response format from Polygon.io bars API");
        }
        
        for (const auto& bar_data : response_json["results"]) {
            if (!bar_data.contains("o") || !bar_data.contains("h") || 
                !bar_data.contains("l") || !bar_data.contains("c") ||
                !bar_data.contains("v") || !bar_data.contains("t")) {
                continue;
            }
            
            Core::Bar bar;
            bar.o = bar_data["o"].get<double>();
            bar.h = bar_data["h"].get<double>();
            bar.l = bar_data["l"].get<double>();
            bar.c = bar_data["c"].get<double>();
            bar.v = bar_data["v"].get<double>();
            bar.t = std::to_string(bar_data["t"].get<long long>());
            
            bars.push_back(bar);
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse Polygon.io bars response: " + std::string(e.what()));
    }
    
    return bars;
}

double PolygonCryptoClient::get_current_price(const std::string& symbol) const {
    if (!is_connected()) {
        throw std::runtime_error("Polygon client not connected");
    }
    
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for price request");
    }
    
    std::lock_guard<std::mutex> lock(data_mutex);
    auto it = latest_prices.find(symbol);
    if (it != latest_prices.end()) {
        return it->second;
    }
    
    std::string url = build_rest_url(config.endpoints.quotes_latest, symbol);
    std::string response = make_authenticated_request(url);
    
    try {
        json response_json = json::parse(response);
        
        if (!response_json.contains("results") || !response_json["results"].is_object()) {
            throw std::runtime_error("Invalid response format from Polygon.io quotes API");
        }
        
        const auto& results = response_json["results"];
        if (!results.contains("last") || !results["last"].contains("price")) {
            throw std::runtime_error("Price data not found in Polygon.io response");
        }
        
        return results["last"]["price"].get<double>();
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse Polygon.io price response: " + std::string(e.what()));
    }
}

Core::QuoteData PolygonCryptoClient::get_realtime_quotes(const std::string& symbol) const {
    if (!is_connected()) {
        throw std::runtime_error("Polygon client not connected");
    }
    
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for quote request");
    }
    
    std::lock_guard<std::mutex> lock(data_mutex);
    auto it = latest_quotes.find(symbol);
    if (it != latest_quotes.end()) {
        return it->second;
    }
    
    std::string url = build_rest_url(config.endpoints.quotes_latest, symbol);
    std::string response = make_authenticated_request(url);
    
    Core::QuoteData quote_data;
    
    try {
        json response_json = json::parse(response);
        
        if (!response_json.contains("results") || !response_json["results"].is_object()) {
            throw std::runtime_error("Invalid response format from Polygon.io quotes API");
        }
        
        const auto& results = response_json["results"];
        if (results.contains("last")) {
            const auto& last = results["last"];
            if (last.contains("price")) {
                double price = last["price"].get<double>();
                quote_data.ask_price = price;
                quote_data.bid_price = price;
                quote_data.mid_price = price;
            }
            if (last.contains("timestamp")) {
                quote_data.timestamp = std::to_string(last["timestamp"].get<long long>());
            }
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse Polygon.io quote response: " + std::string(e.what()));
    }
    
    return quote_data;
}

bool PolygonCryptoClient::is_market_open() const {
    return true;
}

bool PolygonCryptoClient::is_within_trading_hours() const {
    return true;
}

std::string PolygonCryptoClient::get_provider_name() const {
    return "Polygon.io Crypto";
}

Config::ApiProvider PolygonCryptoClient::get_provider_type() const {
    return Config::ApiProvider::POLYGON_CRYPTO;
}

bool PolygonCryptoClient::start_realtime_feed(const std::vector<std::string>& symbols) {
    if (!is_connected()) {
        return false;
    }
    
    if (symbols.empty()) {
        throw std::runtime_error("At least one symbol is required for realtime feed");
    }
    
    if (websocket_active.load()) {
        stop_realtime_feed();
    }
    
    websocket_active = true;
    websocket_thread = std::thread(&PolygonCryptoClient::websocket_worker, this);
    
    return true;
}

void PolygonCryptoClient::stop_realtime_feed() {
    websocket_active = false;
    
    if (websocket_thread.joinable()) {
        websocket_thread.join();
    }
}

void PolygonCryptoClient::websocket_worker() {
    while (websocket_active.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void PolygonCryptoClient::process_websocket_message(const std::string& message) {
    try {
        json msg_json = json::parse(message);
        
        if (msg_json.contains("ev") && msg_json["ev"] == "Q") {
            std::string symbol = msg_json.value("sym", "");
            if (!symbol.empty()) {
                Core::QuoteData quote;
                quote.ask_price = msg_json.value("ap", 0.0);
                quote.bid_price = msg_json.value("bp", 0.0);
                quote.ask_size = msg_json.value("as", 0.0);
                quote.bid_size = msg_json.value("bs", 0.0);
                quote.timestamp = std::to_string(msg_json.value("t", 0LL));
                quote.mid_price = (quote.ask_price + quote.bid_price) / 2.0;
                
                std::lock_guard<std::mutex> lock(data_mutex);
                latest_quotes[symbol] = quote;
                latest_prices[symbol] = quote.mid_price;
            }
        }
        
    } catch (const std::exception& e) {
        // Log error but continue processing
    }
}

std::string PolygonCryptoClient::build_rest_url(const std::string& endpoint, const std::string& symbol) const {
    std::string url = config.base_url + endpoint;
    
    // Replace symbol in URL
    url = replace_url_placeholder(url, symbol);
    
    if (endpoint.find("range") != std::string::npos) {
        // Enforce configured multiplier
        size_t multiplier_pos = url.find("{multiplier}");
        if (multiplier_pos != std::string::npos) {
            if (config.bar_multiplier <= 0) {
                throw std::runtime_error("Polygon bar_multiplier must be configured and > 0");
            }
            std::string multiplier_str = std::to_string(config.bar_multiplier);
            url.replace(multiplier_pos, 12, multiplier_str);
        }
        
        // Enforce configured timespan
        size_t timespan_pos = url.find("{timespan}");
        if (timespan_pos != std::string::npos) {
            if (config.bar_timespan.empty()) {
                throw std::runtime_error("Polygon bar_timespan must be configured (e.g., minute, hour, day)");
            }
            url.replace(timespan_pos, 10, config.bar_timespan);
        }
        
        if (config.bars_range_minutes <= 0) {
            throw std::runtime_error("Polygon bars_range_minutes must be configured and > 0");
        }
        auto now = std::chrono::system_clock::now();
        auto from_time = now - std::chrono::minutes(config.bars_range_minutes);
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        auto from_time_t = std::chrono::system_clock::to_time_t(from_time);
        std::string from_date = std::to_string(from_time_t * 1000);
        std::string to_date = std::to_string(now_time_t * 1000);
        
        size_t from_pos = url.find("{from}");
        if (from_pos != std::string::npos) {
            url.replace(from_pos, 6, from_date);
        }
        
        size_t to_pos = url.find("{to}");
        if (to_pos != std::string::npos) {
            url.replace(to_pos, 4, to_date);
        }
    }
    
    url += (url.find('?') != std::string::npos) ? "&" : "?";
    url += "apikey=" + config.api_key;
    
    return url;
}

std::string PolygonCryptoClient::make_authenticated_request(const std::string& url) const {
    if (url.empty()) {
        throw std::runtime_error("URL is required for authenticated request");
    }
    
    HttpRequest request(url, "", "", "", config.retry_count, config.timeout_seconds, 
                       config.enable_ssl_verification, config.rate_limit_delay_ms, "");
    
    std::string response = http_get(request);
    
    if (response.empty()) {
        throw std::runtime_error("Empty response from Polygon.io API");
    }
    // Basic guard against non-JSON error payloads like "400 Bad Request"
    if (!response.empty() && (response.rfind("Bad Request", 0) == 0 || response.rfind("400", 0) == 0 || response[0] != '{')) {
        throw std::runtime_error("Polygon API returned error payload: " + (response.size() > 64 ? response.substr(0, 64) : response));
    }
    
    return response;
}

bool PolygonCryptoClient::validate_config() const {
    return true;
}

void PolygonCryptoClient::cleanup_resources() {
    stop_realtime_feed();
    
    std::lock_guard<std::mutex> lock(data_mutex);
    latest_quotes.clear();
    latest_prices.clear();
}

} // namespace API
} // namespace AlpacaTrader
