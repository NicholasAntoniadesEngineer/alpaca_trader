#ifndef API_ENDPOINTS_HPP
#define API_ENDPOINTS_HPP

#include <string>
#include "endpoints_config.hpp"

namespace AlpacaTrader {
namespace Config {

/**
 * API Endpoints utility class
 * 
 * This class provides helper functions for URL construction using
 * configuration-loaded endpoint values instead of hardcoded constants.
 */
class ApiEndpoints {
public:
    // Constructor takes the endpoints configuration
    explicit ApiEndpoints(const EndpointsConfig& config) : endpoints(config) {}
    
    // Helper functions for URL construction
    std::string build_trading_url(const std::string& base_url, const std::string& endpoint) const {
        return base_url + endpoint;
    }
    
    std::string build_market_data_url(const std::string& base_url, const std::string& endpoint) const {
        return base_url + endpoint;
    }
    
    std::string build_position_url(const std::string& base_url, const std::string& symbol) const {
        std::string url = base_url + endpoints.trading.position_by_symbol;
        size_t pos = url.find("{symbol}");
        if (pos != std::string::npos) {
            url.replace(pos, 8, symbol);
        }
        return url;
    }
    
    std::string build_bars_url(const std::string& base_url, const std::string& symbol, 
                              const std::string& start, const std::string& end) const {
        std::string url = base_url + endpoints.market_data.bars;
        size_t pos = url.find("{symbol}");
        if (pos != std::string::npos) {
            url.replace(pos, 8, symbol);
        }
        return url + "?start=" + start + "&end=" + end + "&asof=";
    }
    
    std::string build_quotes_latest_url(const std::string& base_url, const std::string& symbol) const {
        std::string url = base_url + endpoints.market_data.quotes_latest;
        size_t pos = url.find("{symbol}");
        if (pos != std::string::npos) {
            url.replace(pos, 8, symbol);
        }
        return url;
    }
    
    std::string build_orders_by_symbol_url(const std::string& base_url, const std::string& symbol) const {
        std::string url = base_url + endpoints.trading.orders_by_symbol;
        size_t pos = url.find("{symbol}");
        if (pos != std::string::npos) {
            url.replace(pos, 8, symbol);
        }
        return url;
    }
    
    // Access to configuration values
    const EndpointsConfig& get_config() const { return endpoints; }

private:
    const EndpointsConfig& endpoints;
};

} // namespace Config
} // namespace AlpacaTrader

#endif // API_ENDPOINTS_HPP
