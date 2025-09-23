#ifndef MARKET_CLOCK_HPP
#define MARKET_CLOCK_HPP

#include "../base/alpaca_base_client.hpp"
#include "../../core/data_structures.hpp"

class MarketClock : public AlpacaBaseClient {
public:
    explicit MarketClock(const AlpacaClientConfig& cfg) : AlpacaBaseClient(cfg) {}
    
    // Market hours and timing operations
    bool is_core_trading_hours() const;
    bool is_within_fetch_window() const;
    
private:
    // Helper methods for time parsing and validation
    std::tm parse_timestamp(const std::string& timestamp) const;
    bool is_within_time_window(int hour, int minute, int open_hour, int open_minute, int close_hour, int close_minute) const;
};

#endif // MARKET_CLOCK_HPP
