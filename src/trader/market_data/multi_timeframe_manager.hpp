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

    // Main data processing methods
    void process_new_second_bar(const MultiTimeframeBar& second_bar);
    void process_new_quote_data(double bid_price, double ask_price, const std::string& timestamp);

    // Historical data loading methods
    void load_historical_data(const std::string& symbol);

    // Data access methods
    const MultiTimeframeData& get_multi_timeframe_data() const { return multi_timeframe_data; }
    MultiTimeframeData& get_multi_timeframe_data() { return multi_timeframe_data; }

    // Partial bar access methods
    std::deque<MultiTimeframeBar> get_bars_with_partial(MthTsTimeframe tf) const;

    // Propagation detection methods
    double get_propagation_score(MthTsTimeframe lower_tf) const;

    // Indicator calculation methods
    void calculate_daily_indicators();
    void calculate_thirty_min_indicators();
    void calculate_minute_indicators();
    void calculate_second_indicators();

    // Force recalculation of all indicators (called every strategy evaluation cycle)
    void recalculate_all_indicators();

    // Aggregation methods
    MultiTimeframeBar aggregate_bars_to_timeframe(const std::deque<MultiTimeframeBar>& source_bars,
                                                 MthTsTimeframe target_timeframe,
                                                 const std::string& target_timestamp) const;

    // Timeframe aggregation methods
    void aggregate_to_minute_bar(const std::string& current_timestamp);
    void aggregate_to_thirty_min_bar(const std::string& current_timestamp);
    void aggregate_to_daily_bar(const std::string& current_timestamp);

    // Indicator evaluation methods
    void evaluate_daily_bias();
    void evaluate_thirty_min_confirmation();
    void evaluate_minute_trigger();
    void evaluate_second_execution_readiness();

    // Utility methods
    bool is_new_timeframe_bar_needed(MthTsTimeframe timeframe, const std::string& current_timestamp) const;
    std::string get_timeframe_start_timestamp(const std::string& current_timestamp) const;

private:
    const SystemConfig& config;
    API::ApiProviderInterface& api_provider;
    MultiTimeframeData multi_timeframe_data;

    // Partial bar management for incremental updates
    MultiTimeframeBar current_minute_bar;
    MultiTimeframeBar current_thirty_min_bar;
    MultiTimeframeBar current_daily_bar;

    std::string current_minute_start_ts;
    std::string current_thirty_min_start_ts;
    std::string current_daily_start_ts;

    size_t current_minute_count;
    size_t current_thirty_min_count;
    size_t current_daily_count;

    // Technical indicator calculation helpers
    double calculate_ema(const std::deque<MultiTimeframeBar>& bars, int period, int ema_period) const;
    double calculate_adx(const std::deque<MultiTimeframeBar>& bars, int period) const;
    double calculate_rsi(const std::deque<MultiTimeframeBar>& bars, int period) const;
    double calculate_atr(const std::deque<MultiTimeframeBar>& bars, int period) const;
    double calculate_volume_ma(const std::deque<MultiTimeframeBar>& bars, int period) const;
    double calculate_average_spread(const std::deque<MultiTimeframeBar>& bars, int period) const;

    // Data management helpers
    void maintain_deque_size(std::deque<MultiTimeframeBar>& deque, size_t max_size);
    bool is_timestamp_in_timeframe(const std::string& timestamp, MthTsTimeframe timeframe,
                                  const std::string& reference_timestamp) const;
    std::chrono::system_clock::time_point parse_timestamp(const std::string& timestamp_str) const;
    std::string format_timestamp(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point get_utc_midnight(const std::chrono::system_clock::time_point& tp) const;

    // Incremental update helpers for partial bars
    void update_minute_with_new_second(const MultiTimeframeBar& second_bar);
    void update_thirty_min_with_new_minute(const MultiTimeframeBar& new_minute_bar);
    void update_daily_with_new_thirty_min(const MultiTimeframeBar& new_thirty_min_bar);

    // Propagation detection helpers
    double calculate_indicator_trend(const std::deque<MultiTimeframeBar>& bars, const std::string& indicator_type, int lookback) const;
    void update_propagation_scores();
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MULTI_TIMEFRAME_MANAGER_HPP
