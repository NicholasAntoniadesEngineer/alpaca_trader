// alpaca_client.hpp
#ifndef ALPACA_CLIENT_HPP
#define ALPACA_CLIENT_HPP

#include "../configs/component_configs.hpp"
#include "../data/data_structures.hpp"
#include "clock/market_clock.hpp"
#include "market/market_data_client.hpp"
#include "orders/order_client.hpp"
#include <vector>

/**
 * AlpacaClient - Unified API facade for Alpaca trading operations
 * 
 * This class provides a simplified interface to all Alpaca API operations
 * by delegating to specialized component classes. It serves as a single
 * configuration point and entry point for all trading-related API calls.
 */
class AlpacaClient {
private:
    MarketClock clock;
    MarketDataClient market_data;
    OrderClient orders;

public:
    explicit AlpacaClient(const AlpacaClientConfig& cfg)
        : clock(cfg), market_data(cfg), orders(cfg) {}

    // Market hours and timing operations
    bool is_core_trading_hours() const { return clock.is_core_trading_hours(); }
    bool is_within_fetch_window() const { return clock.is_within_fetch_window(); }
    
    // Market data operations
    std::vector<Bar> get_recent_bars(const BarRequest& req) const { return market_data.get_recent_bars(req); }
    double get_current_price(const std::string& symbol) const { return market_data.get_current_price(symbol); }
    
    // Order management operations
    void place_bracket_order(const OrderRequest& req) const { orders.place_bracket_order(req); }
    void close_position(const ClosePositionRequest& req) const { orders.close_position(req); }
};

#endif // ALPACA_CLIENT_HPP
