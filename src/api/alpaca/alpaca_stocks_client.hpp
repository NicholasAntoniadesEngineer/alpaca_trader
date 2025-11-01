#ifndef ALPACA_STOCKS_CLIENT_HPP
#define ALPACA_STOCKS_CLIENT_HPP

#include "api/general/api_provider_interface.hpp"
#include "configs/multi_api_config.hpp"
#include "core/trader/data_structures/data_structures.hpp"
#include "core/utils/http_utils.hpp"
#include "core/utils/connectivity_manager.hpp"
#include <string>
#include <vector>

namespace AlpacaTrader {
namespace API {

class AlpacaStocksClient : public ApiProviderInterface {
private:
    Config::ApiProviderConfig config;
    bool connected;
    ConnectivityManager& connectivity_manager;
    
    std::string make_authenticated_request(const std::string& url) const;
    std::string build_url_with_symbol(const std::string& endpoint, const std::string& symbol) const;
    bool validate_config() const;

public:
    explicit AlpacaStocksClient(ConnectivityManager& connectivity_mgr);
    ~AlpacaStocksClient() override;
    
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
};

} // namespace API
} // namespace AlpacaTrader

#endif // ALPACA_STOCKS_CLIENT_HPP
