// alpaca_client.hpp
#ifndef ALPACA_CLIENT_HPP
#define ALPACA_CLIENT_HPP

#include "configs/api_client_config.hpp"
#include "core/trader/data/data_structures.hpp"
#include "market_clock.hpp"
#include "market_data_client.hpp"
#include "order_client.hpp"
#include <vector>

namespace AlpacaTrader {
namespace API {

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
    
    // Direct access to configuration for API methods
    const AlpacaClientConfig config;

public:
    explicit AlpacaClient(const AlpacaClientConfig& cfg);
    ~AlpacaClient();

    // Market hours and timing operations
    bool is_core_trading_hours() const { return clock.is_core_trading_hours(); }
    bool is_within_fetch_window() const { return clock.is_within_fetch_window(); }
    bool is_within_fetch_window(bool is_crypto_asset) const;
    bool is_approaching_market_close() const { return clock.is_approaching_market_close(); }
    int get_minutes_until_market_close() const { return clock.get_minutes_until_market_close(); }

    // Market data operations
    std::vector<Core::Bar> get_recent_bars(const Core::BarRequest& req) const { return market_data.get_recent_bars(req); }
    double get_current_price(const std::string& symbol) const { return market_data.get_current_price(symbol); }
    Core::QuoteData get_realtime_quotes(const std::string& symbol) const { return market_data.get_realtime_quotes(symbol); }

    // Order management operations
    void place_bracket_order(const Core::OrderRequest& req) const { orders.place_bracket_order(req); }
    void close_position(const Core::ClosePositionRequest& req) const { orders.close_position(req); }
    void cancel_orders_for_signal(const std::string& signal_side) const { orders.cancel_orders_for_signal(signal_side); }
    
    // Order management API methods
    std::vector<std::string> get_open_orders(const std::string& symbol) const;
    std::string get_open_orders_json(const std::string& symbol) const;
    void cancel_order(const std::string& order_id) const;
    void cancel_orders_batch(const std::vector<std::string>& order_ids) const;
    bool has_pending_orders(const std::string& symbol) const;
    void cancel_pending_orders(const std::string& symbol) const;
    
    // Position management API methods
    std::string get_positions() const;
    int get_position_quantity(const std::string& symbol) const;
    void submit_market_order(const std::string& symbol, const std::string& side, int quantity) const;
    
    // Short selling validation
    bool check_short_availability(const std::string& symbol, int quantity) const;
    int get_shortable_quantity(const std::string& symbol) const;
};

} // namespace API
} // namespace AlpacaTrader

#endif // ALPACA_CLIENT_HPP
