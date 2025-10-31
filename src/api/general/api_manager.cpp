#include "api_manager.hpp"
#include "api/alpaca/alpaca_trading_client.hpp"
#include "api/alpaca/alpaca_stocks_client.hpp"
#include "api/polygon/polygon_crypto_client.hpp"
#include <stdexcept>
#include <algorithm>

namespace AlpacaTrader {
namespace API {

ApiManager::~ApiManager() {
    shutdown();
}

ApiManager::ApiManager(const Config::MultiApiConfig& multi_config) : config(multi_config) {
    if (config.providers.empty()) {
        throw std::runtime_error("No API providers configured");
    }
    
    for (const auto& [provider_type, provider_config] : config.providers) {
        auto provider = create_provider(provider_type);
        if (!provider) {
            throw std::runtime_error("Failed to create provider");
        }
        
        if (!provider->initialize(provider_config)) {
            throw std::runtime_error("Failed to initialize provider: " + provider->get_provider_name());
        }
        
        providers[provider_type] = std::move(provider);
    }
    
    if (!has_provider(Config::ApiProvider::ALPACA_TRADING)) {
        throw std::runtime_error("Alpaca trading provider is required but not configured");
    }
}

void ApiManager::shutdown() {
    for (auto& [provider_type, provider] : providers) {
        if (provider) {
            provider->disconnect();
        }
    }
    providers.clear();
}

bool ApiManager::has_provider(Config::ApiProvider provider) const {
    return providers.find(provider) != providers.end();
}

ApiProviderInterface* ApiManager::get_provider(Config::ApiProvider provider) const {
    auto it = providers.find(provider);
    if (it == providers.end()) {
        throw std::runtime_error("Provider not found or not initialized");
    }
    
    if (!it->second) {
        throw std::runtime_error("Provider is null - initialization failed");
    }
    
    return it->second.get();
}

std::vector<Core::Bar> ApiManager::get_recent_bars(const Core::BarRequest& request) const {
    if (request.symbol.empty()) {
        throw std::runtime_error("Symbol is required for bar request");
    }
    
    Config::ApiProvider provider = determine_provider_for_symbol(request.symbol);
    return get_provider(provider)->get_recent_bars(request);
}

double ApiManager::get_current_price(const std::string& symbol) const {
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for price request");
    }
    
    Config::ApiProvider provider = determine_provider_for_symbol(symbol);
    return get_provider(provider)->get_current_price(symbol);
}

Core::QuoteData ApiManager::get_realtime_quotes(const std::string& symbol) const {
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for quote request");
    }
    
    Config::ApiProvider provider = determine_provider_for_symbol(symbol);
    return get_provider(provider)->get_realtime_quotes(symbol);
}

bool ApiManager::is_market_open(const std::string& symbol) const {
    if (symbol.empty()) {
        return get_provider(Config::ApiProvider::ALPACA_TRADING)->is_market_open();
    }
    
    Config::ApiProvider provider = determine_provider_for_symbol(symbol);
    return get_provider(provider)->is_market_open();
}

bool ApiManager::is_within_trading_hours(const std::string& symbol) const {
    if (symbol.empty()) {
        return get_provider(Config::ApiProvider::ALPACA_TRADING)->is_within_trading_hours();
    }
    
    Config::ApiProvider provider = determine_provider_for_symbol(symbol);
    return get_provider(provider)->is_within_trading_hours();
}

std::string ApiManager::get_account_info() const {
    auto* trading_provider = dynamic_cast<AlpacaTradingClient*>(get_provider(Config::ApiProvider::ALPACA_TRADING));
    if (!trading_provider) {
        throw std::runtime_error("Trading provider does not support account operations");
    }
    return trading_provider->get_account_info();
}

std::string ApiManager::get_positions() const {
    auto* trading_provider = dynamic_cast<AlpacaTradingClient*>(get_provider(Config::ApiProvider::ALPACA_TRADING));
    if (!trading_provider) {
        throw std::runtime_error("Trading provider does not support position operations");
    }
    return trading_provider->get_positions();
}

std::string ApiManager::get_open_orders() const {
    auto* trading_provider = dynamic_cast<AlpacaTradingClient*>(get_provider(Config::ApiProvider::ALPACA_TRADING));
    if (!trading_provider) {
        throw std::runtime_error("Trading provider does not support order operations");
    }
    return trading_provider->get_open_orders();
}

void ApiManager::place_order(const std::string& order_json) const {
    if (order_json.empty()) {
        throw std::runtime_error("Order JSON is required");
    }
    
    auto* trading_provider = dynamic_cast<AlpacaTradingClient*>(get_provider(Config::ApiProvider::ALPACA_TRADING));
    if (!trading_provider) {
        throw std::runtime_error("Trading provider does not support order placement");
    }
    trading_provider->place_order(order_json);
}

void ApiManager::cancel_order(const std::string& order_id) const {
    if (order_id.empty()) {
        throw std::runtime_error("Order ID is required");
    }
    
    auto* trading_provider = dynamic_cast<AlpacaTradingClient*>(get_provider(Config::ApiProvider::ALPACA_TRADING));
    if (!trading_provider) {
        throw std::runtime_error("Trading provider does not support order cancellation");
    }
    trading_provider->cancel_order(order_id);
}

void ApiManager::close_position(const std::string& symbol, int quantity) const {
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for position closure");
    }
    
    if (quantity == 0) {
        throw std::runtime_error("Quantity must be non-zero for position closure");
    }
    
    auto* trading_provider = dynamic_cast<AlpacaTradingClient*>(get_provider(Config::ApiProvider::ALPACA_TRADING));
    if (!trading_provider) {
        throw std::runtime_error("Trading provider does not support position closure");
    }
    trading_provider->close_position(symbol, quantity);
}

std::vector<Config::ApiProvider> ApiManager::get_active_providers() const {
    std::vector<Config::ApiProvider> active_providers;
    for (const auto& [provider_type, provider] : providers) {
        if (provider && provider->is_connected()) {
            active_providers.push_back(provider_type);
        }
    }
    return active_providers;
}

bool ApiManager::is_crypto_symbol(const std::string& symbol) const {
    if (symbol.empty()) {
        return false;
    }
    
    std::string upper_symbol = symbol;
    std::transform(upper_symbol.begin(), upper_symbol.end(), upper_symbol.begin(), ::toupper);
    
    return upper_symbol.find("BTC") != std::string::npos ||
           upper_symbol.find("ETH") != std::string::npos ||
           upper_symbol.find("USD") != std::string::npos ||
           upper_symbol.find("/") != std::string::npos ||
           upper_symbol.find("-") != std::string::npos;
}

bool ApiManager::is_stock_symbol(const std::string& symbol) const {
    return !is_crypto_symbol(symbol);
}

std::unique_ptr<ApiProviderInterface> ApiManager::create_provider(Config::ApiProvider provider_type) {
    switch (provider_type) {
        case Config::ApiProvider::ALPACA_TRADING:
            return std::make_unique<AlpacaTradingClient>();
        case Config::ApiProvider::ALPACA_STOCKS:
            return std::make_unique<AlpacaStocksClient>();
        case Config::ApiProvider::POLYGON_CRYPTO:
            return std::make_unique<PolygonCryptoClient>();
        default:
            throw std::runtime_error("Unknown API provider type");
    }
}

Config::ApiProvider ApiManager::determine_provider_for_symbol(const std::string& symbol) const {
    if (is_crypto_symbol(symbol)) {
        if (has_provider(Config::ApiProvider::POLYGON_CRYPTO)) {
            return Config::ApiProvider::POLYGON_CRYPTO;
        }
    }
    
    if (has_provider(Config::ApiProvider::ALPACA_STOCKS)) {
        return Config::ApiProvider::ALPACA_STOCKS;
    }
    
    return Config::ApiProvider::ALPACA_TRADING;
}

Config::ApiProvider ApiManager::determine_provider_for_trading() const {
    return Config::ApiProvider::ALPACA_TRADING;
}

} // namespace API
} // namespace AlpacaTrader
