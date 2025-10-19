#ifndef API_MANAGER_HPP
#define API_MANAGER_HPP

#include "api_provider_interface.hpp"
#include "configs/multi_api_config.hpp"
#include "core/trader/data/data_structures.hpp"
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

namespace AlpacaTrader {
namespace API {

class ApiManager {
private:
    std::unordered_map<Config::ApiProvider, std::unique_ptr<ApiProviderInterface>> providers;
    Config::MultiApiConfig config;
    
    std::unique_ptr<ApiProviderInterface> create_provider(Config::ApiProvider provider_type);
    Config::ApiProvider determine_provider_for_symbol(const std::string& symbol) const;
    Config::ApiProvider determine_provider_for_trading() const;

public:
    ApiManager();
    ~ApiManager();
    
    bool initialize(const Config::MultiApiConfig& multi_config);
    void shutdown();
    
    bool has_provider(Config::ApiProvider provider) const;
    ApiProviderInterface* get_provider(Config::ApiProvider provider) const;
    
    std::vector<Core::Bar> get_recent_bars(const Core::BarRequest& request) const;
    double get_current_price(const std::string& symbol) const;
    Core::QuoteData get_realtime_quotes(const std::string& symbol) const;
    
    bool is_market_open(const std::string& symbol = "") const;
    bool is_within_trading_hours(const std::string& symbol = "") const;
    
    std::string get_account_info() const;
    std::string get_positions() const;
    std::string get_open_orders() const;
    void place_order(const std::string& order_json) const;
    void cancel_order(const std::string& order_id) const;
    void close_position(const std::string& symbol, int quantity) const;
    
    std::vector<Config::ApiProvider> get_active_providers() const;
    bool is_crypto_symbol(const std::string& symbol) const;
    bool is_stock_symbol(const std::string& symbol) const;
};

} // namespace API
} // namespace AlpacaTrader

#endif // API_MANAGER_HPP
