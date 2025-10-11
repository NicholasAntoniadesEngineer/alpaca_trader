#ifndef MARKET_CLOCK_HPP
#define MARKET_CLOCK_HPP

#include "alpaca_base_client.hpp"
#include "core/trader/data/data_structures.hpp"

namespace AlpacaTrader {
namespace API {

class MarketClock : public AlpacaBaseClient {
public:
    explicit MarketClock(const AlpacaClientConfig& cfg) : AlpacaBaseClient(cfg) {}

    // Set crypto asset flag
    void set_crypto_asset(bool is_crypto) { crypto_asset = is_crypto; }

    // Market hours and timing operations
    bool is_core_trading_hours() const;
    bool is_within_fetch_window() const;
    bool is_approaching_market_close() const;
    int get_minutes_until_market_close() const;
    
private:
    bool crypto_asset = false; 

    // Helper methods for time parsing and validation
    std::tm parse_timestamp(const std::string& timestamp) const;
    bool is_within_time_window(int hour, int minute, int open_hour, int open_minute, int close_hour, int close_minute) const;
};

} // namespace API
} // namespace AlpacaTrader

#endif // MARKET_CLOCK_HPP
