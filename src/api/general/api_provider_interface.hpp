#ifndef API_PROVIDER_INTERFACE_HPP
#define API_PROVIDER_INTERFACE_HPP

#include "trader/data_structures/data_structures.hpp"
#include "configs/multi_api_config.hpp"
#include <vector>
#include <string>
#include <memory>

namespace AlpacaTrader {
namespace API {

class ApiProviderInterface {
public:
    virtual ~ApiProviderInterface() = default;
    
    virtual bool initialize(const Config::ApiProviderConfig& config) = 0;
    virtual bool is_connected() const = 0;
    virtual void disconnect() = 0;
    
    virtual std::vector<Core::Bar> get_recent_bars(const Core::BarRequest& request) const = 0;
    virtual std::vector<Core::Bar> get_historical_bars(const std::string& symbol, const std::string& timeframe,
                                                      const std::string& start_date, const std::string& end_date,
                                                      int limit = 50000) const = 0;
    virtual double get_current_price(const std::string& symbol) const = 0;
    virtual Core::QuoteData get_realtime_quotes(const std::string& symbol) const = 0;
    
    virtual bool is_market_open() const = 0;
    virtual bool is_within_trading_hours() const = 0;
    
    virtual std::string get_provider_name() const = 0;
    virtual Config::ApiProvider get_provider_type() const = 0;
};

using ApiProviderPtr = std::unique_ptr<ApiProviderInterface>;

} // namespace API
} // namespace AlpacaTrader

#endif // API_PROVIDER_INTERFACE_HPP
