#ifndef MULTI_TIMEFRAME_MANAGER_HPP
#define MULTI_TIMEFRAME_MANAGER_HPP

#include "configs/system_config.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "api/general/api_provider_interface.hpp"
#include "logging/logger/logging_macros.hpp"

using AlpacaTrader::Config::SystemConfig;
using AlpacaTrader::Logging::log_message;

namespace AlpacaTrader {
namespace Core {

class MultiTimeframeManager {
public:
    MultiTimeframeManager(const SystemConfig& config, API::ApiProviderInterface& api_provider);

    void process_new_second_bar(const MultiTimeframeBar& second_bar);
    void process_new_quote_data(double bid_price, double ask_price, const std::string& timestamp);

    void load_historical_data(const std::string& symbol);
    void load_daily_historical_bars(const std::string& symbol, 
                                     const std::chrono::system_clock::time_point& current_time, 
                                     const std::string& end_timestamp);
    void load_minute_historical_bars(const std::string& symbol,
                                      const std::chrono::system_clock::time_point& current_time,
                                      const std::string& end_timestamp);
    void load_second_historical_bars(const std::string& symbol,
                                      const std::chrono::system_clock::time_point& current_time,
                                      const std::string& end_timestamp);
    void generate_thirty_min_bars_from_minute_bars();

    const MultiTimeframeData& get_multi_timeframe_data() const { return multi_timeframe_data; }
    MultiTimeframeData& get_multi_timeframe_data() { return multi_timeframe_data; }

    std::deque<MultiTimeframeBar> get_bars_with_partial(MthTsTimeframe tf) const;

    double get_propagation_score(MthTsTimeframe lower_tf) const;

    MultiTimeframeBar aggregate_bars_to_timeframe(const std::deque<MultiTimeframeBar>& source_bars,
                                                 MthTsTimeframe target_timeframe,
                                                 const std::string& target_timestamp) const;
    MultiTimeframeBar aggregate_consecutive_bars(const std::deque<MultiTimeframeBar>& source_bars,
                                                size_t start_index,
                                                size_t end_index,
                                                const std::string& target_timestamp) const;

    void aggregate_to_minute_bar(const std::string& current_timestamp);
    void aggregate_to_thirty_min_bar(const std::string& current_timestamp);
    void aggregate_to_daily_bar(const std::string& current_timestamp);

    bool is_new_timeframe_bar_needed(MthTsTimeframe timeframe, const std::string& current_timestamp) const;
    std::string get_timeframe_start_timestamp(const std::string& current_timestamp, MthTsTimeframe timeframe) const;

private:
    const SystemConfig& config;
    API::ApiProviderInterface& api_provider;
    MultiTimeframeData multi_timeframe_data;

    MultiTimeframeBar current_minute_bar;
    MultiTimeframeBar current_thirty_min_bar;
    MultiTimeframeBar current_daily_bar;

    std::string current_minute_start_ts;
    std::string current_thirty_min_start_ts;
    std::string current_daily_start_ts;

    size_t current_minute_count;
    int quote_debug_counter;

    void maintain_deque_size(std::deque<MultiTimeframeBar>& bar_deque, size_t maximum_size);
    bool is_timestamp_in_timeframe(const std::string& timestamp, MthTsTimeframe timeframe,
                                  const std::string& reference_timestamp) const;
    std::chrono::system_clock::time_point parse_timestamp(const std::string& timestamp_str) const;
    std::string format_timestamp(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point get_utc_midnight(const std::chrono::system_clock::time_point& tp) const;

    void update_minute_with_new_second(const MultiTimeframeBar& second_bar);
    void update_thirty_min_with_new_minute(const MultiTimeframeBar& new_minute_bar);
    void update_daily_with_new_thirty_min(const MultiTimeframeBar& new_thirty_min_bar);

    void update_propagation_scores();
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MULTI_TIMEFRAME_MANAGER_HPP
