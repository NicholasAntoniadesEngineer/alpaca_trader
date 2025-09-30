// ApiConfig.hpp
#ifndef API_CONFIG_HPP
#define API_CONFIG_HPP

#include <string>
#include "endpoints_config.hpp"

namespace AlpacaTrader {
namespace Config {

struct ApiConfig {
    // Authentication
    std::string api_key;
    std::string api_secret;
    
    // API Endpoints
    std::string base_url;
    std::string data_url;
    
    // HTTP Configuration
    int retry_count;         
    int timeout_seconds;   
    bool enable_ssl_verification; 
    int rate_limit_delay_ms; 
    
    // API Versioning
    std::string api_version;
    
    // Endpoints Configuration
    EndpointsConfig endpoints;
};

} // namespace Config
} // namespace AlpacaTrader

#endif // API_CONFIG_HPP


