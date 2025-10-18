#ifndef MARKET_DATA_CLIENT_HPP
#define MARKET_DATA_CLIENT_HPP

#include "alpaca_base_client.hpp"
#include "core/trader/data/data_structures.hpp"
#include <vector>

namespace AlpacaTrader {
namespace API {

class MarketDataClient : public AlpacaBaseClient {
public:
    explicit MarketDataClient(const AlpacaClientConfig& cfg) : AlpacaBaseClient(cfg) {}
    
    // Market data operations
    std::vector<Core::Bar> get_recent_bars(const Core::BarRequest& req) const;
    
    double get_current_price(const std::string& symbol) const;
    Core::QuoteData get_realtime_quotes(const std::string& symbol) const;
    
private:
    // Helper methods for data fetching
    std::string build_crypto_bars_url(const std::string& symbol, int limit) const;
    std::string build_bars_url(const std::string& symbol, int limit, const std::string& feed) const;
    std::string replace_url_placeholders(const std::string& url, const std::string& symbol, const std::string& timeframe) const;
    std::vector<Core::Bar> parse_bars_response(const std::string& response) const;
    void log_fetch_result(const std::string& description, bool success, size_t bar_count = 0) const;
    void log_fetch_failure() const;
};

} // namespace API
} // namespace AlpacaTrader

#endif // MARKET_DATA_CLIENT_HPP
