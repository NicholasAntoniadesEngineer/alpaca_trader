#ifndef MARKET_DATA_CLIENT_HPP
#define MARKET_DATA_CLIENT_HPP

#include "../base/alpaca_base_client.hpp"
#include "../../data/data_structures.hpp"
#include <vector>

class MarketDataClient : public AlpacaBaseClient {
public:
    explicit MarketDataClient(const AlpacaClientConfig& cfg) : AlpacaBaseClient(cfg) {}
    
    // Market data operations
    std::vector<Bar> get_recent_bars(const BarRequest& req) const;
    
    /**
     * @brief Get real-time current price for a symbol
     * @param symbol The stock symbol (e.g., "AAPL")
     * @return Current ask price, or 0.0 if unavailable
     * @note Uses Alpaca's free real-time quotes API to avoid delayed data issues
     */
    double get_current_price(const std::string& symbol) const;
    
private:
    // Helper methods for data fetching
    std::string build_bars_url(const std::string& symbol, const std::string& start, 
                              const std::string& end, int limit, const std::string& feed) const;
    std::vector<Bar> parse_bars_response(const std::string& response) const;
    void log_fetch_attempt(const std::string& symbol, const std::string& description) const;
    void log_fetch_result(const std::string& description, bool success, size_t bar_count = 0) const;
    void log_fetch_failure() const;
};

#endif // MARKET_DATA_CLIENT_HPP
