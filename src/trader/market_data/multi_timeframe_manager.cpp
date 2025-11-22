#include "multi_timeframe_manager.hpp"
#include "utils/time_utils.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace AlpacaTrader {
namespace Core {

MultiTimeframeManager::MultiTimeframeManager(const SystemConfig& config, API::ApiProviderInterface& api_provider)
    : config(config), api_provider(api_provider), quote_debug_counter(0) {
    try {
        log_message("Initializing Multi-Timeframe Manager for MTH-TS strategy", "");
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to initialize MultiTimeframeManager: " + std::string(e.what()));
    }
}

void MultiTimeframeManager::load_historical_data(const std::string& symbol) {
    try {
        log_message("Loading historical data for MTH-TS strategy - Symbol: " + symbol, "");
        auto current_time = std::chrono::system_clock::now();
        std::string end_timestamp = format_timestamp(current_time);

        load_daily_historical_bars(symbol, current_time, end_timestamp);
        load_minute_historical_bars(symbol, current_time, end_timestamp);
        generate_thirty_min_bars_from_minute_bars();
        load_second_historical_bars(symbol, current_time, end_timestamp);

        log_message("Historical data loading completed for MTH-TS strategy", "");
    } catch (const std::exception& e) {
        log_message("Error loading historical data for MTH-TS: " + std::string(e.what()), "");
    }
}

void MultiTimeframeManager::load_daily_historical_bars(const std::string& symbol, 
                                                        const std::chrono::system_clock::time_point& current_time, 
                                                        const std::string& end_timestamp) {
    if (!config.strategy.mth_ts_daily_enabled) {
        return;
    }

    try {
        auto daily_start_time = current_time - std::chrono::hours(24LL * config.strategy.mth_ts_historical_daily_days);
        std::string daily_start_timestamp = format_timestamp(daily_start_time);
        
        log_message("Daily data request: FROM " + daily_start_timestamp + " TO " + end_timestamp, "");
        log_message("Daily data request: Limit=" + std::to_string(config.strategy.mth_ts_historical_daily_limit) + " bars", "");
        log_message("Daily EMA period: " + std::to_string(config.strategy.mth_ts_daily_ema_period) + " (needs " + 
                   std::to_string(config.strategy.mth_ts_daily_ema_period) + " bars minimum)", "");
        
        auto daily_bars = api_provider.get_historical_bars(symbol, "1day", daily_start_timestamp, end_timestamp,
                                                         config.strategy.mth_ts_historical_daily_limit);
        
        if (!daily_bars.empty()) {
            log_message("Daily data received: " + std::to_string(daily_bars.size()) + " bars", "");
            log_message("Daily time span: " + daily_bars.front().timestamp + " to " + daily_bars.back().timestamp, "");
            
            if (static_cast<int>(daily_bars.size()) < config.strategy.mth_ts_daily_ema_period) {
                log_message("WARNING: Insufficient daily bars for EMA calculation. Have " + 
                           std::to_string(daily_bars.size()) + ", need " + 
                           std::to_string(config.strategy.mth_ts_daily_ema_period), "");
            }
        }
        
        for (const auto& bar : daily_bars) {
            MultiTimeframeBar mtf_bar(bar.open_price, bar.high_price, bar.low_price,
                                     bar.close_price, bar.volume, 0.0, bar.timestamp);
            multi_timeframe_data.daily_bars.push_back(mtf_bar);
        }
        maintain_deque_size(multi_timeframe_data.daily_bars, config.strategy.mth_ts_maintenance_daily_max);
        log_message("Loaded " + std::to_string(multi_timeframe_data.daily_bars.size()) + " daily bars after maintenance", "");
    } catch (const std::exception& e) {
        log_message("Failed to load daily historical bars: " + std::string(e.what()), "");
    }
}

void MultiTimeframeManager::load_minute_historical_bars(const std::string& symbol,
                                                         const std::chrono::system_clock::time_point& current_time,
                                                         const std::string& end_timestamp) {
    if (!config.strategy.mth_ts_1min_enabled) {
        return;
    }

    try {
        auto minute_start_time = current_time - std::chrono::hours(24LL * config.strategy.mth_ts_historical_1min_days);
        std::string minute_start_timestamp = format_timestamp(minute_start_time);
        
        auto minute_bars = api_provider.get_historical_bars(symbol, "1min", minute_start_timestamp, end_timestamp,
                                                     config.strategy.mth_ts_historical_1min_limit);
        for (const auto& bar : minute_bars) {
            MultiTimeframeBar mtf_bar(bar.open_price, bar.high_price, bar.low_price,
                                     bar.close_price, bar.volume, 0.0, bar.timestamp);
            multi_timeframe_data.minute_bars.push_back(mtf_bar);
        }
        maintain_deque_size(multi_timeframe_data.minute_bars, config.strategy.mth_ts_maintenance_1min_max);
        log_message("Loaded " + std::to_string(multi_timeframe_data.minute_bars.size()) + " 1-minute bars", "");
    } catch (const std::exception& e) {
        log_message("Failed to load 1-minute historical bars: " + std::string(e.what()), "");
    }
}

void MultiTimeframeManager::generate_thirty_min_bars_from_minute_bars() {
    if (multi_timeframe_data.minute_bars.empty()) {
        return;
    }

    try {
        log_message("Generating initial 30-minute bars from 1-minute historical data...", "");
        
        const size_t bars_per_thirty_min = 30;
        size_t minute_bars_count = multi_timeframe_data.minute_bars.size();
        size_t thirty_min_bars_generated = 0;
        
        for (size_t i = bars_per_thirty_min; i <= minute_bars_count; i += bars_per_thirty_min) {
            MultiTimeframeBar thirty_min_bar = aggregate_consecutive_bars(
                multi_timeframe_data.minute_bars,
                i - bars_per_thirty_min,
                i,
                multi_timeframe_data.minute_bars[i-1].timestamp
            );
            
            multi_timeframe_data.thirty_min_bars.push_back(thirty_min_bar);
            thirty_min_bars_generated++;
        }
        
        maintain_deque_size(multi_timeframe_data.thirty_min_bars, config.strategy.mth_ts_maintenance_30min_max);
        log_message("Generated " + std::to_string(thirty_min_bars_generated) + " 30-minute bars from 1-minute data", "");
        log_message("Kept " + std::to_string(multi_timeframe_data.thirty_min_bars.size()) + " 30-minute bars after maintenance", "");
    } catch (const std::exception& e) {
        log_message("Failed to generate 30-minute bars: " + std::string(e.what()), "");
    }
}

void MultiTimeframeManager::load_second_historical_bars(const std::string& symbol,
                                                         const std::chrono::system_clock::time_point& current_time,
                                                         const std::string& end_timestamp) {
    if (!config.strategy.mth_ts_1sec_enabled) {
        return;
    }

    try {
        auto second_start_time = current_time - std::chrono::hours(config.strategy.mth_ts_historical_1sec_hours);
        std::string second_start_timestamp = format_timestamp(second_start_time);
        
        auto second_bars = api_provider.get_historical_bars(symbol, "1sec", second_start_timestamp, end_timestamp,
                                                     config.strategy.mth_ts_historical_1sec_limit);
        for (const auto& bar : second_bars) {
            MultiTimeframeBar mtf_bar(bar.open_price, bar.high_price, bar.low_price,
                                     bar.close_price, bar.volume, 0.0, bar.timestamp);
            multi_timeframe_data.second_bars.push_back(mtf_bar);
        }
        maintain_deque_size(multi_timeframe_data.second_bars, config.strategy.mth_ts_maintenance_1sec_max);
        log_message("Loaded " + std::to_string(multi_timeframe_data.second_bars.size()) + " 1-second bars", "");
    } catch (const std::exception& e) {
        log_message("Failed to load 1-second historical bars: " + std::string(e.what()), "");
    }
}

void MultiTimeframeManager::process_new_second_bar(const MultiTimeframeBar& second_bar) {
    try {
        multi_timeframe_data.second_bars.push_back(second_bar);
        maintain_deque_size(multi_timeframe_data.second_bars, config.strategy.mth_ts_maintenance_1sec_max);
        update_minute_with_new_second(second_bar);
        update_propagation_scores();
    } catch (const std::exception& e) {
        std::cerr << "MTH-TS: Failed to process new second bar: " << e.what() << std::endl;
        throw std::runtime_error("Failed to process second bar: " + std::string(e.what()));
    }
}

void MultiTimeframeManager::process_new_quote_data(double bid_price, double ask_price, const std::string& /*timestamp*/) {
    try {
        if (bid_price <= 0.0 || ask_price <= 0.0 || bid_price >= ask_price) {
            return;
        }

        if (!multi_timeframe_data.second_bars.empty()) {
            double spread_percentage = ((ask_price - bid_price) / bid_price) * 100.0;
            multi_timeframe_data.second_bars.back().spread = spread_percentage;

            if (++quote_debug_counter % config.strategy.mth_ts_spread_debug_log_interval == 0) {
                std::ostringstream debug_message;
                debug_message << "MTH-TS SPREAD DEBUG: Bid=" << bid_price
                             << " Ask=" << ask_price
                             << " Spread=$" << (ask_price - bid_price)
                             << " Spread%=" << spread_percentage << "%";
                log_message(debug_message.str(), "");
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "MTH-TS: Failed to process quote data: " << e.what() << std::endl;
        throw std::runtime_error("Failed to process quote data: " + std::string(e.what()));
    }
}

MultiTimeframeBar MultiTimeframeManager::aggregate_consecutive_bars(
    const std::deque<MultiTimeframeBar>& source_bars,
    size_t start_index,
    size_t end_index,
    const std::string& target_timestamp) const {

    if (source_bars.empty() || start_index >= end_index || end_index > source_bars.size()) {
        throw std::runtime_error("Invalid bar range for aggregation");
    }

    MultiTimeframeBar aggregated_bar;
    aggregated_bar.timestamp = target_timestamp;
    aggregated_bar.open_price = source_bars[start_index].open_price;
    aggregated_bar.close_price = source_bars[end_index - 1].close_price;
    aggregated_bar.high_price = source_bars[start_index].high_price;
    aggregated_bar.low_price = source_bars[start_index].low_price;
    aggregated_bar.volume = 0.0;
    aggregated_bar.spread = 0.0;

    for (size_t i = start_index; i < end_index; ++i) {
        aggregated_bar.high_price = std::max(aggregated_bar.high_price, source_bars[i].high_price);
        aggregated_bar.low_price = std::min(aggregated_bar.low_price, source_bars[i].low_price);
        aggregated_bar.volume += source_bars[i].volume;
        aggregated_bar.spread += source_bars[i].spread;
    }
    
    size_t bar_count = end_index - start_index;
    aggregated_bar.spread = (bar_count > 0) ? (aggregated_bar.spread / bar_count) : 0.0;

    return aggregated_bar;
}

MultiTimeframeBar MultiTimeframeManager::aggregate_bars_to_timeframe(
    const std::deque<MultiTimeframeBar>& source_bars,
    MthTsTimeframe target_timeframe,
    const std::string& target_timestamp) const {

    try {
        if (source_bars.empty()) {
            throw std::runtime_error("No source bars available for aggregation");
        }

        size_t bars_to_aggregate = 0;
        switch (target_timeframe) {
            case MthTsTimeframe::MINUTE_1:
                bars_to_aggregate = 60;
                break;
            case MthTsTimeframe::MINUTE_30:
                bars_to_aggregate = 30;
                break;
            case MthTsTimeframe::DAILY:
                bars_to_aggregate = 48;
                break;
            default:
                bars_to_aggregate = source_bars.size();
                break;
        }

        size_t start_index = (source_bars.size() > bars_to_aggregate) ?
                          source_bars.size() - bars_to_aggregate : 0;

        if (start_index >= source_bars.size()) {
            throw std::runtime_error("Insufficient bars for timeframe aggregation");
        }

        return aggregate_consecutive_bars(source_bars, start_index, source_bars.size(), target_timestamp);

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error aggregating bars to timeframe: " << e.what();
        log_message(error_stream.str(), "");
        throw std::runtime_error(error_stream.str());
    }
}

void MultiTimeframeManager::maintain_deque_size(std::deque<MultiTimeframeBar>& bar_deque, size_t maximum_size) {
    try {
        while (bar_deque.size() > maximum_size) {
            bar_deque.pop_front();
        }
    } catch (const std::exception& e) {
        std::cerr << "MTH-TS: Error in maintain_deque_size: " << e.what() << std::endl;
        throw std::runtime_error("Failed to maintain deque size: " + std::string(e.what()));
    }
}

bool MultiTimeframeManager::is_new_timeframe_bar_needed(MthTsTimeframe timeframe, const std::string& current_timestamp) const {
    try {
        if (timeframe == MthTsTimeframe::DAILY) {
            if (multi_timeframe_data.daily_bars.empty()) {
                return true;
            }

            std::tm current_tm = {};
            std::istringstream ss_current(current_timestamp);
            ss_current >> std::get_time(&current_tm, "%Y-%m-%dT%H:%M:%S");

            const auto& last_daily_bar = multi_timeframe_data.daily_bars.back();
            std::tm last_tm = {};
            std::istringstream ss_last(last_daily_bar.timestamp);
            ss_last >> std::get_time(&last_tm, "%Y-%m-%dT%H:%M:%S");

            return current_tm.tm_mday != last_tm.tm_mday ||
                   current_tm.tm_mon != last_tm.tm_mon ||
                   current_tm.tm_year != last_tm.tm_year;
        }

        return false;

    } catch (const std::exception& e) {
        return false;
    }
}

void MultiTimeframeManager::aggregate_to_minute_bar(const std::string& current_timestamp) {
    try {
        if (multi_timeframe_data.second_bars.size() < 60) {
            return;
        }

        MultiTimeframeBar minute_bar = aggregate_bars_to_timeframe(
            multi_timeframe_data.second_bars,
            MthTsTimeframe::MINUTE_1,
            current_timestamp
        );

        multi_timeframe_data.minute_bars.push_back(minute_bar);
        maintain_deque_size(multi_timeframe_data.minute_bars, config.strategy.mth_ts_maintenance_1min_max);

        aggregate_to_thirty_min_bar(current_timestamp);

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error aggregating to minute bar: " << e.what();
        log_message(error_stream.str(), "");
        throw std::runtime_error(error_stream.str());
    }
}

void MultiTimeframeManager::aggregate_to_thirty_min_bar(const std::string& current_timestamp) {
    try {
        if (multi_timeframe_data.minute_bars.size() < 30) {
            return;
        }

        MultiTimeframeBar thirty_min_bar = aggregate_bars_to_timeframe(
            multi_timeframe_data.minute_bars,
            MthTsTimeframe::MINUTE_30,
            current_timestamp
        );

        multi_timeframe_data.thirty_min_bars.push_back(thirty_min_bar);
        maintain_deque_size(multi_timeframe_data.thirty_min_bars, config.strategy.mth_ts_maintenance_30min_max);

        aggregate_to_daily_bar(current_timestamp);

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error aggregating to thirty-minute bar: " << e.what();
        log_message(error_stream.str(), "");
        throw std::runtime_error(error_stream.str());
    }
}

void MultiTimeframeManager::aggregate_to_daily_bar(const std::string& current_timestamp) {
    try {
        if (!is_new_timeframe_bar_needed(MthTsTimeframe::DAILY, current_timestamp)) {
            return;
        }

        if (multi_timeframe_data.thirty_min_bars.size() < 48) {
            return;
        }

        MultiTimeframeBar daily_bar = aggregate_bars_to_timeframe(
            multi_timeframe_data.thirty_min_bars,
            MthTsTimeframe::DAILY,
            current_timestamp
        );

        multi_timeframe_data.daily_bars.push_back(daily_bar);
        maintain_deque_size(multi_timeframe_data.daily_bars, config.strategy.mth_ts_maintenance_daily_max);

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error aggregating to daily bar: " << e.what();
        log_message(error_stream.str(), "");
        throw std::runtime_error(error_stream.str());
    }
}

std::string MultiTimeframeManager::get_timeframe_start_timestamp(const std::string& current_timestamp, MthTsTimeframe timeframe) const {
    try {
        long long timestamp_milliseconds = std::stoll(current_timestamp);
        long long timestamp_seconds = timestamp_milliseconds / TimeUtils::MILLISECONDS_PER_SECOND;
        long long rounded_seconds;

        switch (timeframe) {
            case MthTsTimeframe::MINUTE_1:
                rounded_seconds = (timestamp_seconds / TimeUtils::SECONDS_PER_MINUTE) * TimeUtils::SECONDS_PER_MINUTE;
                break;
            case MthTsTimeframe::MINUTE_30:
                {
                    const long long seconds_per_thirty_minutes = TimeUtils::SECONDS_PER_MINUTE * 30;
                    rounded_seconds = (timestamp_seconds / seconds_per_thirty_minutes) * seconds_per_thirty_minutes;
                }
                break;
            case MthTsTimeframe::DAILY:
                {
                    auto time_point = std::chrono::system_clock::time_point(std::chrono::milliseconds(timestamp_milliseconds));
                    auto midnight_time_point = get_utc_midnight(time_point);
                    rounded_seconds = std::chrono::duration_cast<std::chrono::seconds>(midnight_time_point.time_since_epoch()).count();
                }
                break;
            default:
                rounded_seconds = timestamp_seconds;
                break;
        }

        return std::to_string(rounded_seconds * TimeUtils::MILLISECONDS_PER_SECOND);
    } catch (const std::exception& parse_exception) {
        std::ostringstream error_stream;
        error_stream << "Failed to parse timestamp for timeframe rounding. Timestamp: " 
                    << current_timestamp << " Error: " << parse_exception.what();
        log_message(error_stream.str(), "");
        throw std::runtime_error(error_stream.str());
    }
}

std::string MultiTimeframeManager::format_timestamp(const std::chrono::system_clock::time_point& time_point) const {
    auto duration = time_point.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return std::to_string(milliseconds);
}

std::chrono::system_clock::time_point MultiTimeframeManager::parse_timestamp(const std::string& timestamp_string) const {
    try {
        long long milliseconds = std::stoll(timestamp_string);
        auto duration = std::chrono::milliseconds(milliseconds);
        return std::chrono::system_clock::time_point(duration);
    } catch (const std::exception& e) {
        log_message("Error parsing timestamp: " + std::string(e.what()), "");
        return std::chrono::system_clock::time_point();
    }
}

std::chrono::system_clock::time_point MultiTimeframeManager::get_utc_midnight(const std::chrono::system_clock::time_point& time_point) const {
    auto time_t_value = std::chrono::system_clock::to_time_t(time_point);

    std::tm tm_value = *std::gmtime(&time_t_value);

    tm_value.tm_hour = 0;
    tm_value.tm_min = 0;
    tm_value.tm_sec = 0;

    return std::chrono::system_clock::from_time_t(std::mktime(&tm_value));
}

bool MultiTimeframeManager::is_timestamp_in_timeframe(const std::string& timestamp, MthTsTimeframe timeframe,
                                                      const std::string& reference_timestamp) const {
    try {
        auto timestamp_point = parse_timestamp(timestamp);
        auto reference_point = parse_timestamp(reference_timestamp);

        switch (timeframe) {
            case MthTsTimeframe::SECOND_1:
                return std::chrono::duration_cast<std::chrono::seconds>(timestamp_point - reference_point).count() == 0;

            case MthTsTimeframe::MINUTE_1:
                return std::chrono::duration_cast<std::chrono::minutes>(timestamp_point - reference_point).count() == 0;

            case MthTsTimeframe::MINUTE_30:
                {
                    auto reference_minutes = std::chrono::duration_cast<std::chrono::minutes>(reference_point.time_since_epoch()).count();
                    auto timestamp_minutes = std::chrono::duration_cast<std::chrono::minutes>(timestamp_point.time_since_epoch()).count();
                    return (reference_minutes / 30) == (timestamp_minutes / 30);
                }

            case MthTsTimeframe::DAILY:
                return get_utc_midnight(timestamp_point) == get_utc_midnight(reference_point);

            default:
                return false;
        }
    } catch (const std::exception& e) {
        log_message("Error in is_timestamp_in_timeframe: " + std::string(e.what()), "");
        return false;
    }
}

std::deque<MultiTimeframeBar> MultiTimeframeManager::get_bars_with_partial(MthTsTimeframe timeframe) const {
    std::deque<MultiTimeframeBar> result;

    switch (timeframe) {
        case MthTsTimeframe::DAILY:
            result = multi_timeframe_data.daily_bars;
            if (!current_daily_start_ts.empty()) {
                result.push_back(current_daily_bar);
            }
            break;
        case MthTsTimeframe::MINUTE_30:
            result = multi_timeframe_data.thirty_min_bars;
            if (!current_thirty_min_start_ts.empty()) {
                result.push_back(current_thirty_min_bar);
            }
            break;
        case MthTsTimeframe::MINUTE_1:
            result = multi_timeframe_data.minute_bars;
            if (!current_minute_start_ts.empty()) {
                result.push_back(current_minute_bar);
            }
            break;
        case MthTsTimeframe::SECOND_1:
            result = multi_timeframe_data.second_bars;
            break;
    }

    return result;
}

double MultiTimeframeManager::get_propagation_score(MthTsTimeframe lower_timeframe) const {
    switch (lower_timeframe) {
        case MthTsTimeframe::MINUTE_1:
            return multi_timeframe_data.minute_to_thirty_min_propagation_score;
        case MthTsTimeframe::SECOND_1:
            return multi_timeframe_data.second_to_minute_propagation_score;
        default:
            return 0.0;
    }
}

void MultiTimeframeManager::update_minute_with_new_second(const MultiTimeframeBar& second_bar) {
    try {
        std::string minute_start_timestamp = get_timeframe_start_timestamp(second_bar.timestamp, MthTsTimeframe::MINUTE_1);

        if (current_minute_start_ts.empty() || !is_timestamp_in_timeframe(second_bar.timestamp, MthTsTimeframe::MINUTE_1, current_minute_start_ts)) {
            if (!current_minute_start_ts.empty()) {
                log_message("Completing 1-minute bar at " + current_minute_bar.timestamp + 
                           " (from " + std::to_string(current_minute_count) + " second bars)", "");
                
                multi_timeframe_data.minute_bars.push_back(current_minute_bar);
                maintain_deque_size(multi_timeframe_data.minute_bars, config.strategy.mth_ts_maintenance_1min_max);
                
                log_message("1-minute bars: " + std::to_string(multi_timeframe_data.minute_bars.size()) + " total", "");

                update_thirty_min_with_new_minute(current_minute_bar);
            }

            current_minute_bar = second_bar;
            current_minute_start_ts = minute_start_timestamp;
            current_minute_count = 1;
        } else {
            current_minute_bar.high_price = std::max(current_minute_bar.high_price, second_bar.high_price);
            current_minute_bar.low_price = std::min(current_minute_bar.low_price, second_bar.low_price);
            current_minute_bar.close_price = second_bar.close_price;
            current_minute_bar.volume += second_bar.volume;

            double total_spread = current_minute_bar.spread * current_minute_count;
            total_spread += second_bar.spread;
            current_minute_count++;
            current_minute_bar.spread = total_spread / current_minute_count;
        }
    } catch (const std::exception& e) {
        std::cerr << "MTH-TS: Failed to update minute with new second: " << e.what() << std::endl;
        throw std::runtime_error("Failed to update minute bar: " + std::string(e.what()));
    }
}

void MultiTimeframeManager::update_thirty_min_with_new_minute(const MultiTimeframeBar& new_minute_bar) {
    try {
        const size_t required_minute_bars_for_thirty_min = 30;
        
        if (multi_timeframe_data.minute_bars.size() < required_minute_bars_for_thirty_min) {
            return;
        }

        size_t start_index = multi_timeframe_data.minute_bars.size() - required_minute_bars_for_thirty_min;
        MultiTimeframeBar rolling_thirty_min_bar = aggregate_consecutive_bars(
            multi_timeframe_data.minute_bars,
            start_index,
            multi_timeframe_data.minute_bars.size(),
            new_minute_bar.timestamp
        );

        multi_timeframe_data.thirty_min_bars.push_back(rolling_thirty_min_bar);
        maintain_deque_size(multi_timeframe_data.thirty_min_bars, config.strategy.mth_ts_maintenance_30min_max);

        log_message("Rolling 30-minute bar updated from last " + std::to_string(required_minute_bars_for_thirty_min) + " minute bars", "");
        log_message("30-minute bars: " + std::to_string(multi_timeframe_data.thirty_min_bars.size()) + " total", "");

        update_daily_with_new_thirty_min(rolling_thirty_min_bar);

    } catch (const std::exception& aggregation_exception) {
        std::ostringstream error_stream;
        error_stream << "Failed to update rolling 30-min window: " << aggregation_exception.what();
        log_message(error_stream.str(), "");
        throw std::runtime_error(error_stream.str());
    }
}

void MultiTimeframeManager::update_daily_with_new_thirty_min(const MultiTimeframeBar& new_thirty_min_bar) {
    try {
        const size_t required_thirty_min_bars_for_daily = 48;
        
        if (multi_timeframe_data.thirty_min_bars.size() < required_thirty_min_bars_for_daily) {
            return;
        }

        size_t start_index = multi_timeframe_data.thirty_min_bars.size() - required_thirty_min_bars_for_daily;
        MultiTimeframeBar rolling_daily_bar = aggregate_consecutive_bars(
            multi_timeframe_data.thirty_min_bars,
            start_index,
            multi_timeframe_data.thirty_min_bars.size(),
            new_thirty_min_bar.timestamp
        );

        multi_timeframe_data.daily_bars.push_back(rolling_daily_bar);
        maintain_deque_size(multi_timeframe_data.daily_bars, config.strategy.mth_ts_maintenance_daily_max);

        log_message("Rolling daily bar updated from last " + std::to_string(required_thirty_min_bars_for_daily) + " thirty-min bars", "");

    } catch (const std::exception& aggregation_exception) {
        std::ostringstream error_stream;
        error_stream << "Failed to update rolling daily window: " << aggregation_exception.what();
        log_message(error_stream.str(), "");
        throw std::runtime_error(error_stream.str());
    }
}

void MultiTimeframeManager::update_propagation_scores() {
    try {
        auto minute_bars_with_partial = get_bars_with_partial(MthTsTimeframe::MINUTE_1);
        auto thirty_min_bars_with_partial = get_bars_with_partial(MthTsTimeframe::MINUTE_30);

        double second_to_minute_score = 0.0;
        if (minute_bars_with_partial.size() >= 2) {
            const auto& current_minute = minute_bars_with_partial.back();
            const auto& previous_minute = minute_bars_with_partial[minute_bars_with_partial.size() - 2];

            bool price_momentum = current_minute.close_price > previous_minute.close_price;
            bool volume_momentum = current_minute.volume > previous_minute.volume;

            if (price_momentum && volume_momentum) {
                second_to_minute_score = 0.8;
            } else if (price_momentum || volume_momentum) {
                second_to_minute_score = 0.5;
            }
        }
        multi_timeframe_data.second_to_minute_propagation_score = second_to_minute_score;

        double minute_to_thirty_score = 0.0;
        if (thirty_min_bars_with_partial.size() >= 2) {
            const auto& current_thirty = thirty_min_bars_with_partial.back();
            const auto& previous_thirty = thirty_min_bars_with_partial[thirty_min_bars_with_partial.size() - 2];

            bool price_trend = current_thirty.close_price > previous_thirty.close_price;

            if (price_trend) {
                minute_to_thirty_score = 0.7;
            }
        }
        multi_timeframe_data.minute_to_thirty_min_propagation_score = minute_to_thirty_score;

    } catch (const std::exception& e) {
        std::cerr << "MTH-TS: Error updating propagation scores: " << e.what() << std::endl;
        throw std::runtime_error("Failed to update propagation scores: " + std::string(e.what()));
    }
}

} // namespace Core
} // namespace AlpacaTrader

