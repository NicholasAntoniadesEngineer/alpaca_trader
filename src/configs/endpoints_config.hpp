#ifndef ENDPOINTS_CONFIG_HPP
#define ENDPOINTS_CONFIG_HPP

#include <string>

namespace AlpacaTrader {
namespace Config {

/**
 * Endpoints configuration structure
 * 
 * This structure holds all API endpoint configurations loaded from CSV
 */
struct EndpointsConfig {
    // Base URLs for different API services
    std::string trading_paper_url;
    std::string trading_live_url;
    std::string market_data_url;
    
    // API Version
    std::string api_version;
    
    // Trading API endpoints (only used ones)
    struct Trading {
        std::string account;
        std::string positions;
        std::string position_by_symbol;
        std::string orders;
        std::string orders_by_symbol;
        std::string clock;
    } trading;
    
    // Market Data API endpoints (only used ones)
    struct MarketData {
        std::string bars;
        std::string quotes_latest;
    } market_data;
    
    // Default constructor
    EndpointsConfig() = default;
};

} // namespace Config
} // namespace AlpacaTrader

#endif // ENDPOINTS_CONFIG_HPP
