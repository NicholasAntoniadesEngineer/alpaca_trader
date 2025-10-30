#ifndef MULTI_API_CONFIG_HPP
#define MULTI_API_CONFIG_HPP

#include <string>
#include <unordered_map>

namespace AlpacaTrader {
namespace Config {

enum class ApiProvider {
    ALPACA_TRADING,
    ALPACA_STOCKS,
    POLYGON_CRYPTO
};

struct ApiProviderConfig {
    std::string api_key;
    std::string api_secret;
    std::string base_url;
    std::string websocket_url;
    int retry_count;
    int timeout_seconds;
    bool enable_ssl_verification;
    int rate_limit_delay_ms;
    std::string api_version;
    
    // Bar configuration (for providers that support configurable bars)
    std::string bar_timespan;
    int bar_multiplier;
    int bars_range_minutes;
    
    struct EndpointConfig {
        std::string bars;
        std::string quotes_latest;
        std::string trades;
        std::string account;
        std::string positions;
        std::string orders;
        std::string clock;
        std::string assets;
    } endpoints;
};

struct MultiApiConfig {
    std::unordered_map<ApiProvider, ApiProviderConfig> providers;
    
    bool has_provider(ApiProvider provider) const {
        return providers.find(provider) != providers.end();
    }
    
    const ApiProviderConfig& get_provider_config(ApiProvider provider) const {
        auto it = providers.find(provider);
        if (it == providers.end()) {
            throw std::runtime_error("API provider configuration not found");
        }
        return it->second;
    }
    
    ApiProviderConfig& get_provider_config(ApiProvider provider) {
        auto it = providers.find(provider);
        if (it == providers.end()) {
            throw std::runtime_error("API provider configuration not found");
        }
        return it->second;
    }
};

} // namespace Config
} // namespace AlpacaTrader

#endif // MULTI_API_CONFIG_HPP
