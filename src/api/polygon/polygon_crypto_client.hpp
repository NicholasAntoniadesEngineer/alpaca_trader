#ifndef POLYGON_CRYPTO_CLIENT_HPP
#define POLYGON_CRYPTO_CLIENT_HPP

#include "api/general/api_provider_interface.hpp"
#include "configs/multi_api_config.hpp"
#include "configs/system_config.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "utils/http_utils.hpp"
#include "utils/connectivity_manager.hpp"
#include "api/polygon/websocket_client.hpp"
#include "api/polygon/bar_accumulator.hpp"
#include "trader/market_data/multi_timeframe_manager.hpp"
#include "json/json.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <memory>
#include <chrono>

using json = nlohmann::json;

namespace AlpacaTrader {
namespace API {

class PolygonCryptoClient : public ApiProviderInterface {
private:
    Config::ApiProviderConfig config;
    std::atomic<bool> connected{false};
    std::atomic<bool> websocket_active{false};
    ConnectivityManager& connectivity_manager;
    
    std::unique_ptr<Polygon::WebSocketClient> websocketClientPointer;
    mutable std::mutex data_mutex;
    std::condition_variable data_condition;
    
    mutable std::unordered_map<std::string, Core::QuoteData> latest_quotes;
    mutable std::unordered_map<std::string, double> latest_prices;
    mutable std::unordered_map<std::string, Core::Bar> latest_bars;
    mutable std::unordered_map<std::string, std::unique_ptr<Polygon::BarAccumulator>> barAccumulatorMap;

    std::unique_ptr<Core::MultiTimeframeManager> multi_timeframe_manager;
    
    std::vector<std::string> subscribed_symbols;
    
    mutable std::chrono::steady_clock::time_point lastStaleDataLogTime;

    // Polling thread for reliable data collection
    std::atomic<bool> polling_active{false};
    std::thread polling_thread;
    std::mutex polling_mutex;
    mutable std::string lastStaleDataTimestamp;
    mutable std::unordered_map<std::string, std::vector<Core::Bar>> lastLoggedBarsMap;
    mutable std::chrono::steady_clock::time_point lastBarsLogTime;
    std::atomic<long long> messages_processed{0};
    
    bool process_websocket_message(const std::string& message);
    bool process_single_message(const nlohmann::json& msg_json);
    bool process_bar_message(const nlohmann::json& msg_json);
    bool process_quote_message(const nlohmann::json& msg_json);
    std::string convert_symbol_for_websocket(const std::string& symbol) const;  
    std::string build_rest_url(const std::string& endpoint, const std::string& symbol) const;
    std::string make_authenticated_request(const std::string& url) const;
    std::string convert_symbol_to_polygon_format(const std::string& symbol) const;
    
    bool validate_config() const;
    void cleanup_resources();

public:
    explicit PolygonCryptoClient(ConnectivityManager& connectivity_mgr);
    ~PolygonCryptoClient() override;
    
    bool initialize(const Config::ApiProviderConfig& config) override;
    bool is_connected() const override;
    void disconnect() override;
    
    std::vector<Core::Bar> get_recent_bars(const Core::BarRequest& request) const override;
    std::vector<Core::Bar> get_historical_bars(const std::string& symbol, const std::string& timeframe,
                                               const std::string& start_date, const std::string& end_date,
                                               int limit = 50000) const override;
    double get_current_price(const std::string& symbol) const override;
    Core::QuoteData get_realtime_quotes(const std::string& symbol) const override;
    
    bool is_market_open() const override;
    bool is_within_trading_hours() const override;
    
    std::string get_provider_name() const override;
    Config::ApiProvider get_provider_type() const override;
    
    bool start_realtime_feed(const std::vector<std::string>& symbols);
    void stop_realtime_feed();
    bool is_websocket_active() const { return websocket_active.load(); }

    void initialize_multi_timeframe_manager(const Config::SystemConfig& system_config);
    Core::MultiTimeframeManager* get_multi_timeframe_manager() const { return multi_timeframe_manager.get(); }

    // Polling fallback for reliable data collection
    void startPollingThread();
    void stopPollingThread();
    void pollingThreadFunction();
};

} // namespace API
} // namespace AlpacaTrader

#endif // POLYGON_CRYPTO_CLIENT_HPP
