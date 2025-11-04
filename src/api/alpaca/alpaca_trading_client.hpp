#ifndef ALPACA_TRADING_CLIENT_HPP
#define ALPACA_TRADING_CLIENT_HPP

#include "api/general/api_provider_interface.hpp"
#include "configs/multi_api_config.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "utils/http_utils.hpp"
#include "utils/connectivity_manager.hpp"
#include <string>
#include <vector>

namespace AlpacaTrader {
namespace API {

class AlpacaTradingClient : public ApiProviderInterface {
private:
    Config::ApiProviderConfig config;
    bool connected;
    ConnectivityManager& connectivity_manager;
    
    std::string make_authenticated_request(const std::string& request_url, const std::string& method, 
                                         const std::string& request_body) const;
    std::string build_url(const std::string& endpoint) const;
    std::string build_url_with_symbol(const std::string& endpoint, const std::string& symbol) const;
    bool validate_config() const;

public:
    explicit AlpacaTradingClient(ConnectivityManager& connectivity_mgr);
    ~AlpacaTradingClient() override;
    
    bool initialize(const Config::ApiProviderConfig& config) override;
    bool is_connected() const override;
    void disconnect() override;
    
    std::vector<Core::Bar> get_recent_bars(const Core::BarRequest& request) const override;
    double get_current_price(const std::string& symbol) const override;
    Core::QuoteData get_realtime_quotes(const std::string& symbol) const override;
    
    bool is_market_open() const override;
    bool is_within_trading_hours() const override;
    
    std::string get_provider_name() const override;
    Config::ApiProvider get_provider_type() const override;
    
    std::string get_account_info() const;
    std::string get_positions() const;
    std::string get_open_orders() const;
    void place_order(const std::string& order_json) const;
    void cancel_order(const std::string& order_id) const;
    void close_position(const std::string& symbol, int quantity) const;
};

} // namespace API
} // namespace AlpacaTrader

#endif // ALPACA_TRADING_CLIENT_HPP
