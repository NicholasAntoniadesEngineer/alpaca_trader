#ifndef API_CONFIG_HPP
#define API_CONFIG_HPP

#include <string>

namespace AlpacaTrader {
namespace Config {

struct ApiConfig {
    // Authentication
    std::string api_key;
    std::string api_secret;
    
    // Base URLs for different API services
    std::string base_url;              // Primary trading API URL (paper or live)
    std::string data_url;              // Market data API URL
    std::string trading_paper_url;     // Paper trading URL
    std::string trading_live_url;      // Live trading URL
    std::string market_data_url;       // Market data URL
    
    // HTTP Configuration
    int retry_count;
    int timeout_seconds;
    bool enable_ssl_verification;
    int rate_limit_delay_ms;
    
    // API Versioning
    std::string api_version;
    
    // Endpoints Configuration
    struct {
        struct {
            std::string account;
            std::string positions;
            std::string position_by_symbol;
            std::string orders;
            std::string orders_by_symbol;
            std::string clock;
        } trading;
        
        struct {
            std::string bars;
            std::string quotes_latest;
        } market_data;
    } endpoints;
};

} // namespace Config
} // namespace AlpacaTrader

#endif // API_CONFIG_HPP


