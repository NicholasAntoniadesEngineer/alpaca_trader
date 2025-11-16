#include "multi_timeframe_manager.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace AlpacaTrader {
namespace Core {

MultiTimeframeManager::MultiTimeframeManager(const SystemConfig& config, API::ApiProviderInterface& api_provider)
    : config(config), api_provider(api_provider) {
    try {
        log_message("Initializing Multi-Timeframe Manager for MTH-TS strategy", "");
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to initialize MultiTimeframeManager: " + std::string(e.what()));
    }
}

void MultiTimeframeManager::load_historical_data(const std::string& symbol) {
    try {
        log_message("Loading historical data for MTH-TS strategy - Symbol: " + symbol, "");

        // Calculate date ranges for historical data using configured parameters
        auto now = std::chrono::system_clock::now();

        // Daily data: Load configured number of days for EMA calculation
        auto daily_start = now - std::chrono::hours(24LL * config.strategy.mth_ts_historical_daily_days);
        std::string daily_start_str = format_timestamp(daily_start);

        // 30-minute data: Load configured number of days
        auto thirty_min_start = now - std::chrono::hours(24LL * config.strategy.mth_ts_historical_30min_days);
        std::string thirty_min_start_str = format_timestamp(thirty_min_start);

        // 1-minute data: Load configured number of days
        auto minute_start = now - std::chrono::hours(24LL * config.strategy.mth_ts_historical_1min_days);
        std::string minute_start_str = format_timestamp(minute_start);

        // 1-second data: Load configured number of hours
        auto second_start = now - std::chrono::hours(config.strategy.mth_ts_historical_1sec_hours);
        std::string second_start_str = format_timestamp(second_start);

        std::string end_date = format_timestamp(now);

        // Load daily bars
        if (config.strategy.mth_ts_daily_enabled) {
            try {
                auto daily_bars = api_provider.get_historical_bars(symbol, "1day", daily_start_str, end_date,
                                                                 config.strategy.mth_ts_historical_daily_limit);
                for (const auto& bar : daily_bars) {
                    MultiTimeframeBar mtf_bar(bar.open_price, bar.high_price, bar.low_price,
                                             bar.close_price, bar.volume, 0.0, bar.timestamp);
                    multi_timeframe_data.daily_bars.push_back(mtf_bar);
                }
                maintain_deque_size(multi_timeframe_data.daily_bars, config.strategy.mth_ts_maintenance_daily_max);
                log_message("Loaded " + std::to_string(multi_timeframe_data.daily_bars.size()) + " daily bars", "");
            } catch (const std::exception& e) {
                log_message("Failed to load daily historical bars: " + std::string(e.what()), "");
            }
        }

        // Load 30-minute bars
        if (config.strategy.mth_ts_30min_enabled) {
            try {
                auto thirty_min_bars = api_provider.get_historical_bars(symbol, "30min", thirty_min_start_str, end_date,
                                                                  config.strategy.mth_ts_historical_30min_limit);
                for (const auto& bar : thirty_min_bars) {
                    MultiTimeframeBar mtf_bar(bar.open_price, bar.high_price, bar.low_price,
                                             bar.close_price, bar.volume, 0.0, bar.timestamp);
                    multi_timeframe_data.thirty_min_bars.push_back(mtf_bar);
                }
                maintain_deque_size(multi_timeframe_data.thirty_min_bars, config.strategy.mth_ts_maintenance_30min_max);
                log_message("Loaded " + std::to_string(multi_timeframe_data.thirty_min_bars.size()) + " 30-minute bars", "");
            } catch (const std::exception& e) {
                log_message("Failed to load 30-minute historical bars: " + std::string(e.what()), "");
            }
        }

        // Load 1-minute bars
        if (config.strategy.mth_ts_1min_enabled) {
            try {
                auto minute_bars = api_provider.get_historical_bars(symbol, "1min", minute_start_str, end_date,
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

        // Load 1-second bars (last 2 hours for initial indicators)
        if (config.strategy.mth_ts_1sec_enabled) {
            try {
                auto second_bars = api_provider.get_historical_bars(symbol, "1sec", second_start_str, end_date,
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

        // Calculate initial indicators for all timeframes
        calculate_daily_indicators();
        calculate_thirty_min_indicators();
        calculate_minute_indicators();
        calculate_second_indicators();

        // Evaluate initial bias and confirmation
        evaluate_daily_bias();
        evaluate_thirty_min_confirmation();
        evaluate_minute_trigger();
        evaluate_second_execution_readiness();

        log_message("Historical data loading completed for MTH-TS strategy", "");

    } catch (const std::exception& e) {
        log_message("Error loading historical data for MTH-TS: " + std::string(e.what()), "");
    }
}

void MultiTimeframeManager::process_new_second_bar(const MultiTimeframeBar& second_bar) {
    // ABSOLUTE DEFENSIVE PROGRAMMING - NO EXCEPTIONS CAN ESCAPE THIS FUNCTION
    try {
        // Add the new 1-second bar to our data
        try {
            multi_timeframe_data.second_bars.push_back(second_bar);
            maintain_deque_size(multi_timeframe_data.second_bars, config.strategy.mth_ts_maintenance_1sec_max);
        } catch (const std::exception& e) {
            std::cerr << "MTH-TS: Failed to add second bar to deque: " << e.what() << std::endl;
            return;
        } catch (...) {
            std::cerr << "MTH-TS: Unknown error adding second bar to deque" << std::endl;
            return;
        }

        // INCREMENTAL UPDATE: Update minute bar with new second bar
        try {
            update_minute_with_new_second(second_bar);
        } catch (const std::exception& e) {
            std::cerr << "MTH-TS: Failed to update minute with new second: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "MTH-TS: Unknown error updating minute with new second" << std::endl;
        }

        // Update propagation scores for hybrid evaluation
        try {
            update_propagation_scores();
        } catch (const std::exception& e) {
            std::cerr << "MTH-TS: Failed to update propagation scores: " << e.what() << std::endl;
        }

        // CRITICAL: Recalculate ALL timeframe indicators with every new second bar
        // This ensures all MTH-TS layers use the latest data including partial bars
        try {
            // Recalculate indicators for all timeframes (they now use get_bars_with_partial internally)
            calculate_daily_indicators();
            evaluate_daily_bias();

            calculate_thirty_min_indicators();
            evaluate_thirty_min_confirmation();

            calculate_minute_indicators();
            evaluate_minute_trigger();

            calculate_second_indicators();
            evaluate_second_execution_readiness();
        } catch (const std::exception& e) {
            std::cerr << "MTH-TS: Failed to recalculate all indicators: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "MTH-TS: Unknown error recalculating all indicators" << std::endl;
        }

    } catch (...) {
        // ABSOLUTE CATCH-ALL - NO EXCEPTIONS CAN ESCAPE THIS FUNCTION
        std::cerr << "MTH-TS: CRITICAL - Unknown exception in process_new_second_bar - system must continue" << std::endl;
    }
}

void MultiTimeframeManager::process_new_quote_data(double bid_price, double ask_price, const std::string& /*timestamp*/) {
    // ABSOLUTE DEFENSIVE PROGRAMMING - NO EXCEPTIONS CAN ESCAPE THIS FUNCTION
    try {
        if (bid_price <= 0.0 || ask_price <= 0.0 || bid_price >= ask_price) {
            return; // Invalid quote data
        }

        try {
            // Update spread in the latest second bar if it exists
            if (!multi_timeframe_data.second_bars.empty()) {
                double spread_pct = ((ask_price - bid_price) / bid_price) * 100.0;
                multi_timeframe_data.second_bars.back().spread = spread_pct;

                // DEBUG: Log spread calculation details periodically
                static int debug_counter = 0;
                if (++debug_counter % 100 == 0) {  // Log every 100 quotes
                    std::ostringstream debug_msg;
                    debug_msg << "MTH-TS SPREAD DEBUG: Bid=" << bid_price
                             << " Ask=" << ask_price
                             << " Spread=$" << (ask_price - bid_price)
                             << " Spread%=" << spread_pct << "%";
                    log_message(debug_msg.str(), "");
                }
            }
        } catch (...) {
            std::cerr << "MTH-TS: Failed to update spread in quote data" << std::endl;
        }

    } catch (...) {
        // ABSOLUTE CATCH-ALL - NO EXCEPTIONS CAN ESCAPE THIS FUNCTION
        std::cerr << "MTH-TS: CRITICAL - Unknown exception in process_new_quote_data - system must continue" << std::endl;
    }
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
                bars_to_aggregate = 60; // 60 seconds
                break;
            case MthTsTimeframe::MINUTE_30:
                bars_to_aggregate = 30; // 30 minutes
                break;
            case MthTsTimeframe::DAILY:
                bars_to_aggregate = 48; // 48 * 30min = 24 hours (assuming 30-min source)
                break;
            default:
                bars_to_aggregate = source_bars.size();
                break;
        }

        // Get the last N bars for aggregation
        size_t start_idx = (source_bars.size() > bars_to_aggregate) ?
                          source_bars.size() - bars_to_aggregate : 0;

        if (start_idx >= source_bars.size()) {
            throw std::runtime_error("Insufficient bars for timeframe aggregation");
        }

        MultiTimeframeBar aggregated_bar;
        aggregated_bar.open_price = source_bars[start_idx].open_price;
        aggregated_bar.high_price = source_bars[start_idx].high_price;
        aggregated_bar.low_price = source_bars[start_idx].low_price;
        aggregated_bar.close_price = source_bars.back().close_price;
        aggregated_bar.volume = 0.0;

        // Calculate average spread from source bars
        double total_spread = 0.0;
        size_t spread_count = 0;
        for (size_t i = start_idx; i < source_bars.size(); ++i) {
            if (source_bars[i].spread > 0.0) {  // Only count valid spreads
                total_spread += source_bars[i].spread;
                spread_count++;
            }
        }
        aggregated_bar.spread = (spread_count > 0) ? (total_spread / spread_count) : 0.0;

        aggregated_bar.timestamp = target_timestamp;

        // Aggregate OHLC and volume
        for (size_t i = start_idx; i < source_bars.size(); ++i) {
            aggregated_bar.high_price = std::max(aggregated_bar.high_price, source_bars[i].high_price);
            aggregated_bar.low_price = std::min(aggregated_bar.low_price, source_bars[i].low_price);
            aggregated_bar.volume += source_bars[i].volume;
            aggregated_bar.spread += source_bars[i].spread;
        }

        // Average spread
        if (source_bars.size() > start_idx) {
            aggregated_bar.spread /= (source_bars.size() - start_idx);
        }

        return aggregated_bar;

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error aggregating bars to timeframe: " << e.what();
        log_message(error_stream.str(), "");
        return MultiTimeframeBar();
    }
}

void MultiTimeframeManager::calculate_daily_indicators() {
    try {
        // Use bars with partial to include unfinished data
        auto daily_bars_with_partial = get_bars_with_partial(MthTsTimeframe::DAILY);

        if (static_cast<int>(daily_bars_with_partial.size()) < config.strategy.mth_ts_daily_ema_period) {
            return; // Insufficient data
        }

        // Calculate 200-EMA
        multi_timeframe_data.daily_indicators.ema = calculate_ema(
            daily_bars_with_partial,
            daily_bars_with_partial.size(),
            config.strategy.mth_ts_daily_ema_period
        );

        // Calculate ADX(14)
        multi_timeframe_data.daily_indicators.adx = calculate_adx(
            daily_bars_with_partial,
            config.strategy.mth_ts_daily_adx_period
        );

        // Calculate ATR
        multi_timeframe_data.daily_indicators.atr = calculate_atr(
            daily_bars_with_partial,
            14 // ATR period
        );

        // Calculate volume moving average
        multi_timeframe_data.daily_indicators.volume_ma = calculate_volume_ma(
            daily_bars_with_partial,
            config.strategy.mth_ts_daily_volume_ma_period
        );

        // Calculate average spread
        multi_timeframe_data.daily_indicators.spread_avg = calculate_average_spread(
            daily_bars_with_partial,
            10 // Last 10 days
        );

        // Determine bullish bias
        if (!multi_timeframe_data.daily_bars.empty()) {
            double latest_close = multi_timeframe_data.daily_bars.back().close_price;
            bool ema_alignment = latest_close > multi_timeframe_data.daily_indicators.ema;
            bool adx_threshold = multi_timeframe_data.daily_indicators.adx >= config.strategy.mth_ts_daily_adx_threshold;
            bool spread_ok = multi_timeframe_data.daily_indicators.spread_avg <= config.strategy.mth_ts_daily_avg_spread_threshold;


            multi_timeframe_data.daily_bullish_bias = ema_alignment && adx_threshold && spread_ok;
        }

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error calculating daily indicators: " << e.what();
        log_message(error_stream.str(), "");
    }
}

void MultiTimeframeManager::calculate_thirty_min_indicators() {
    try {
        // Use bars with partial to include unfinished data
        auto thirty_min_bars_with_partial = get_bars_with_partial(MthTsTimeframe::MINUTE_30);

        if (static_cast<int>(thirty_min_bars_with_partial.size()) < config.strategy.mth_ts_30min_ema_period) {
            return; // Insufficient data
        }

        // Calculate 50-EMA
        multi_timeframe_data.thirty_min_indicators.ema = calculate_ema(
            thirty_min_bars_with_partial,
            thirty_min_bars_with_partial.size(),
            config.strategy.mth_ts_30min_ema_period
        );

        // Calculate ADX(14)
        multi_timeframe_data.thirty_min_indicators.adx = calculate_adx(
            thirty_min_bars_with_partial,
            config.strategy.mth_ts_30min_adx_period
        );

        // Calculate ATR
        multi_timeframe_data.thirty_min_indicators.atr = calculate_atr(
            thirty_min_bars_with_partial,
            14 // ATR period
        );

        // Calculate volume moving average
        multi_timeframe_data.thirty_min_indicators.volume_ma = calculate_volume_ma(
            thirty_min_bars_with_partial,
            config.strategy.mth_ts_30min_volume_ma_period
        );

        // Calculate average spread
        multi_timeframe_data.thirty_min_indicators.spread_avg = calculate_average_spread(
            thirty_min_bars_with_partial,
            20 // Last 20 bars
        );

        // Determine 30-min confirmation
        if (!multi_timeframe_data.thirty_min_bars.empty() && multi_timeframe_data.daily_bullish_bias) {
            double latest_close = multi_timeframe_data.thirty_min_bars.back().close_price;
            bool ema_alignment = latest_close > multi_timeframe_data.thirty_min_indicators.ema;
            bool adx_threshold = multi_timeframe_data.thirty_min_indicators.adx >= config.strategy.mth_ts_30min_adx_threshold;
            bool spread_ok = multi_timeframe_data.thirty_min_indicators.spread_avg <= config.strategy.mth_ts_30min_avg_spread_threshold;

            multi_timeframe_data.thirty_min_confirmation = ema_alignment && adx_threshold && spread_ok;
        } else {
            multi_timeframe_data.thirty_min_confirmation = false;
        }

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error calculating 30-min indicators: " << e.what();
        log_message(error_stream.str(), "");
    }
}

void MultiTimeframeManager::calculate_minute_indicators() {
    try {
        // Use bars with partial to include unfinished data
        auto minute_bars_with_partial = get_bars_with_partial(MthTsTimeframe::MINUTE_1);

        if (static_cast<int>(minute_bars_with_partial.size()) < config.strategy.mth_ts_1min_slow_ema_period) {
            return; // Insufficient data
        }

        // Calculate 9-EMA and 21-EMA
        double ema9 = calculate_ema(minute_bars_with_partial,
                                   minute_bars_with_partial.size(),
                                   config.strategy.mth_ts_1min_fast_ema_period);
        double ema21 = calculate_ema(minute_bars_with_partial,
                                    minute_bars_with_partial.size(),
                                    config.strategy.mth_ts_1min_slow_ema_period);

        // Store the fast EMA (9-period) as the main EMA for crossover checks
        multi_timeframe_data.minute_indicators.ema = ema9;

        // Calculate RSI(14)
        multi_timeframe_data.minute_indicators.rsi = calculate_rsi(
            minute_bars_with_partial,
            config.strategy.mth_ts_1min_rsi_period
        );

        // Calculate ATR
        multi_timeframe_data.minute_indicators.atr = calculate_atr(
            minute_bars_with_partial,
            14 // ATR period
        );

        // Calculate volume MA(20)
        multi_timeframe_data.minute_indicators.volume_ma = calculate_volume_ma(
            minute_bars_with_partial,
            config.strategy.mth_ts_1min_volume_ma_period
        );

        // Calculate average spread
        multi_timeframe_data.minute_indicators.spread_avg = calculate_average_spread(
            minute_bars_with_partial,
            20 // Last 20 bars
        );

        // Determine minute trigger signal
        if (!multi_timeframe_data.minute_bars.empty() && multi_timeframe_data.thirty_min_confirmation) {
            bool ema_crossover = ema9 > ema21;
            double rsi_threshold = (multi_timeframe_data.minute_indicators.spread_avg > config.strategy.mth_ts_1min_spread_threshold) ?
                                  config.strategy.mth_ts_1min_rsi_threshold_strict :
                                  config.strategy.mth_ts_1min_rsi_threshold;
            bool rsi_condition = multi_timeframe_data.minute_indicators.rsi < rsi_threshold;
            bool volume_condition = multi_timeframe_data.minute_bars.back().volume >
                                  (multi_timeframe_data.minute_indicators.volume_ma * config.strategy.mth_ts_1min_volume_multiplier);

            multi_timeframe_data.minute_trigger_signal = ema_crossover && rsi_condition && volume_condition;
        } else {
            multi_timeframe_data.minute_trigger_signal = false;
        }

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error calculating minute indicators: " << e.what();
        log_message(error_stream.str(), "");
    }
}

void MultiTimeframeManager::calculate_second_indicators() {
    try {
        if (static_cast<int>(multi_timeframe_data.second_bars.size()) < config.strategy.mth_ts_1sec_ema_period) {
            return; // Insufficient data
        }

        // Calculate EMA(5)
        multi_timeframe_data.second_indicators.ema = calculate_ema(
            multi_timeframe_data.second_bars,
            multi_timeframe_data.second_bars.size(),
            config.strategy.mth_ts_1sec_ema_period
        );

        // Calculate ATR
        multi_timeframe_data.second_indicators.atr = calculate_atr(
            multi_timeframe_data.second_bars,
            14 // ATR period
        );

        // Calculate volume MA
        multi_timeframe_data.second_indicators.volume_ma = calculate_volume_ma(
            multi_timeframe_data.second_bars,
            20 // Volume MA period
        );

        // Calculate average spread
        multi_timeframe_data.second_indicators.spread_avg = calculate_average_spread(
            multi_timeframe_data.second_bars,
            10 // Last 10 seconds
        );

        // Determine execution readiness
        if (!multi_timeframe_data.second_bars.empty() && multi_timeframe_data.minute_trigger_signal) {
            // Check momentum over last 3 bars
            bool has_momentum = true;
            size_t bars_to_check = std::min(static_cast<size_t>(config.strategy.mth_ts_1sec_momentum_bars),
                                          multi_timeframe_data.second_bars.size());

            for (size_t i = multi_timeframe_data.second_bars.size() - bars_to_check;
                 i < multi_timeframe_data.second_bars.size() - 1; ++i) {
                if (multi_timeframe_data.second_bars[i+1].close_price <= multi_timeframe_data.second_bars[i].close_price) {
                    has_momentum = false;
                    break;
                }
            }

            bool spread_ok = multi_timeframe_data.second_indicators.spread_avg <= config.strategy.mth_ts_1sec_spread_threshold;

            multi_timeframe_data.second_execution_ready = has_momentum && spread_ok;
        } else {
            multi_timeframe_data.second_execution_ready = false;
        }

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error calculating second indicators: " << e.what();
        log_message(error_stream.str(), "");
    }
}

void MultiTimeframeManager::recalculate_all_indicators() {
    try {
        // DEPRECATED: This method now simply calls individual indicator calculations
        // The higher timeframe bars are now maintained incrementally via partial bars
        // All calculations now use get_bars_with_partial() internally

        calculate_daily_indicators();
        evaluate_daily_bias();

        calculate_thirty_min_indicators();
        evaluate_thirty_min_confirmation();

        calculate_minute_indicators();
        evaluate_minute_trigger();

        calculate_second_indicators();
        evaluate_second_execution_readiness();

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error recalculating all indicators: " << e.what();
        log_message(error_stream.str(), "");
    }
}

// Technical indicator calculation implementations
double MultiTimeframeManager::calculate_ema(const std::deque<MultiTimeframeBar>& bars, int /*period*/, int ema_period) const {
    try {
        if (static_cast<int>(bars.size()) < ema_period || ema_period <= 0) return 0.0;

        // Calculate SMA for the first ema_period bars
        double sma = 0.0;
        for (int i = 0; i < ema_period; ++i) {
            sma += bars[bars.size() - ema_period + i].close_price;
        }
        sma /= ema_period;

        // If we only have exactly ema_period bars, return the SMA
        if (static_cast<int>(bars.size()) == ema_period) {
            return sma;
        }

        // Start EMA calculation from the SMA
        double ema = sma;
        double multiplier = 2.0 / (ema_period + 1.0);

        // Apply EMA formula to subsequent bars
        for (size_t i = bars.size() - ema_period + 1; i < bars.size(); ++i) {
            ema = (bars[i].close_price * multiplier) + (ema * (1.0 - multiplier));
        }

        return ema;
    } catch (const std::exception& e) {
        return 0.0;
    }
}

double MultiTimeframeManager::calculate_adx(const std::deque<MultiTimeframeBar>& bars, int period) const {
    try {
        if (static_cast<int>(bars.size()) < period + 1) return 0.0;

        // Simplified ADX calculation - full implementation would be more complex
        // This is a basic approximation
        double adx_sum = 0.0;
        int count = 0;

        for (size_t i = 1; i < bars.size(); ++i) {  // Start from 1 to access i-1 safely
            double tr = std::max({bars[i].high_price - bars[i].low_price,
                                std::abs(bars[i].high_price - bars[i-1].close_price),
                                std::abs(bars[i].low_price - bars[i-1].close_price)});

            if (tr > 0.0) {
                double dm_plus = (bars[i].high_price > bars[i-1].high_price) ?
                               std::max(bars[i].high_price - bars[i-1].high_price, 0.0) : 0.0;
                double dm_minus = (bars[i-1].low_price > bars[i].low_price) ?
                                std::max(bars[i-1].low_price - bars[i].low_price, 0.0) : 0.0;

                double di_plus = (dm_plus / tr) * 100.0;
                double di_minus = (dm_minus / tr) * 100.0;

                // Prevent division by zero in DX calculation
                double di_sum = di_plus + di_minus;
                double dx = (di_sum > 0.0) ? (std::abs(di_plus - di_minus) / di_sum) * 100.0 : 0.0;

                // Ensure DX is valid (should be between 0-100)
                if (!std::isnan(dx) && !std::isinf(dx)) {
                    adx_sum += dx;
                    count++;
                }
            }
        }

        return (count > 0) ? adx_sum / count : 0.0;
    } catch (const std::exception& e) {
        return 0.0;
    }
}

double MultiTimeframeManager::calculate_rsi(const std::deque<MultiTimeframeBar>& bars, int period) const {
    try {
        if (static_cast<int>(bars.size()) < period + 1) return 50.0; // Neutral RSI

        double gains = 0.0, losses = 0.0;

        for (size_t i = bars.size() - period; i < bars.size(); ++i) {
            double change = bars[i].close_price - bars[i-1].close_price;
            if (change > 0) gains += change;
            else losses += std::abs(change);
        }

        if (losses == 0.0) return 100.0;
        double rs = gains / losses;
        return 100.0 - (100.0 / (1.0 + rs));
    } catch (const std::exception& e) {
        return 50.0;
    }
}

double MultiTimeframeManager::calculate_volume_ma(const std::deque<MultiTimeframeBar>& bars, int period) const {
    try {
        if (static_cast<int>(bars.size()) < period) return 0.0;

        double volume_sum = 0.0;
        for (size_t i = bars.size() - period; i < bars.size(); ++i) {
            volume_sum += bars[i].volume;
        }

        return volume_sum / period;
    } catch (const std::exception& e) {
        return 0.0;
    }
}

double MultiTimeframeManager::calculate_average_spread(const std::deque<MultiTimeframeBar>& bars, int period) const {
    try {
        if (static_cast<int>(bars.size()) < period) return 0.0;

        double spread_sum = 0.0;
        for (size_t i = bars.size() - period; i < bars.size(); ++i) {
            spread_sum += bars[i].spread;
        }

        return spread_sum / period;
    } catch (const std::exception& e) {
        return 0.0;
    }
}

double MultiTimeframeManager::calculate_atr(const std::deque<MultiTimeframeBar>& bars, int period) const {
    try {
        if (static_cast<int>(bars.size()) < period + 1) return 0.0;

        std::vector<double> true_ranges;

        // Calculate True Range for each bar
        for (size_t i = 1; i < bars.size(); ++i) {
            double tr = std::max({
                bars[i].high_price - bars[i].low_price,  // Current range
                std::abs(bars[i].high_price - bars[i-1].close_price),  // Gap up
                std::abs(bars[i].low_price - bars[i-1].close_price)   // Gap down
            });
            true_ranges.push_back(tr);
        }

        if (true_ranges.size() < static_cast<size_t>(period)) return 0.0;

        // Calculate ATR as simple moving average of true ranges
        double atr_sum = 0.0;
        for (size_t i = true_ranges.size() - period; i < true_ranges.size(); ++i) {
            atr_sum += true_ranges[i];
        }

        return atr_sum / period;
    } catch (const std::exception& e) {
        return 0.0;
    }
}

void MultiTimeframeManager::maintain_deque_size(std::deque<MultiTimeframeBar>& deque, size_t max_size) {
    try {
        while (deque.size() > max_size) {
            deque.pop_front();
        }
    } catch (const std::exception& e) {
        std::cerr << "MTH-TS: Error in maintain_deque_size: " << e.what() << std::endl;
        // Don't re-throw to prevent memory corruption cascade
    } catch (...) {
        std::cerr << "MTH-TS: Unknown error in maintain_deque_size" << std::endl;
        // Don't re-throw to prevent memory corruption cascade
    }
}

bool MultiTimeframeManager::is_new_timeframe_bar_needed(MthTsTimeframe timeframe, const std::string& current_timestamp) const {
    try {
        // For daily bars, check if we've crossed UTC midnight
        if (timeframe == MthTsTimeframe::DAILY) {
            if (multi_timeframe_data.daily_bars.empty()) return true;

            // Parse current timestamp
            std::tm current_tm = {};
            std::istringstream ss_current(current_timestamp);
            ss_current >> std::get_time(&current_tm, "%Y-%m-%dT%H:%M:%S");

            // Parse last daily bar timestamp
            const auto& last_daily = multi_timeframe_data.daily_bars.back();
            std::tm last_tm = {};
            std::istringstream ss_last(last_daily.timestamp);
            ss_last >> std::get_time(&last_tm, "%Y-%m-%dT%H:%M:%S");

            // Check if day has changed
            return current_tm.tm_mday != last_tm.tm_mday ||
                   current_tm.tm_mon != last_tm.tm_mon ||
                   current_tm.tm_year != last_tm.tm_year;
        }

        return false; // Other timeframes handled by aggregation logic

    } catch (const std::exception& e) {
        return false;
    }
}

void MultiTimeframeManager::aggregate_to_minute_bar(const std::string& current_timestamp) {
    try {
        // Check if we have enough second bars to create a minute bar (60 seconds)
        if (multi_timeframe_data.second_bars.size() < 60) {
            return;
        }

        // Aggregate the last 60 seconds into a minute bar
        MultiTimeframeBar minute_bar = aggregate_bars_to_timeframe(
            multi_timeframe_data.second_bars,
            MthTsTimeframe::MINUTE_1,
            current_timestamp
        );

        // Add to minute bars deque
        multi_timeframe_data.minute_bars.push_back(minute_bar);
        maintain_deque_size(multi_timeframe_data.minute_bars, 1800); // Keep last 30 hours

        // Recalculate minute indicators and evaluate trigger
        calculate_minute_indicators();
        evaluate_minute_trigger();

        // Trigger aggregation to higher timeframes if needed
        aggregate_to_thirty_min_bar(current_timestamp);

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error aggregating to minute bar: " << e.what();
        log_message(error_stream.str(), "");
    }
}

void MultiTimeframeManager::aggregate_to_thirty_min_bar(const std::string& current_timestamp) {
    try {
        // Check if we have enough minute bars to create a 30-minute bar (30 minutes)
        if (multi_timeframe_data.minute_bars.size() < 30) {
            return;
        }

        // Aggregate the last 30 minutes into a 30-minute bar
        MultiTimeframeBar thirty_min_bar = aggregate_bars_to_timeframe(
            multi_timeframe_data.minute_bars,
            MthTsTimeframe::MINUTE_30,
            current_timestamp
        );

        // Add to thirty-minute bars deque
        multi_timeframe_data.thirty_min_bars.push_back(thirty_min_bar);
        maintain_deque_size(multi_timeframe_data.thirty_min_bars, 336); // Keep last 7 days

        // Recalculate thirty-minute indicators
        calculate_thirty_min_indicators();
        evaluate_thirty_min_confirmation();

        // Trigger aggregation to daily timeframe if needed
        aggregate_to_daily_bar(current_timestamp);

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error aggregating to thirty-minute bar: " << e.what();
        log_message(error_stream.str(), "");
    }
}

void MultiTimeframeManager::aggregate_to_daily_bar(const std::string& current_timestamp) {
    try {
        // For daily bars, we need to check if we've crossed midnight UTC
        if (!is_new_timeframe_bar_needed(MthTsTimeframe::DAILY, current_timestamp)) {
            return;
        }

        // Check if we have enough thirty-minute bars for a full day (48 * 30min = 24 hours)
        if (multi_timeframe_data.thirty_min_bars.size() < 48) {
            return;
        }

        // Aggregate the last 48 thirty-minute bars into a daily bar
        MultiTimeframeBar daily_bar = aggregate_bars_to_timeframe(
            multi_timeframe_data.thirty_min_bars,
            MthTsTimeframe::DAILY,
            current_timestamp
        );

        // Add to daily bars deque
        multi_timeframe_data.daily_bars.push_back(daily_bar);
        maintain_deque_size(multi_timeframe_data.daily_bars, 30); // Keep last 30 days

        // Recalculate daily indicators
        calculate_daily_indicators();
        evaluate_daily_bias();

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error aggregating to daily bar: " << e.what();
        log_message(error_stream.str(), "");
    }
}

void MultiTimeframeManager::evaluate_daily_bias() {
    try {
        // Update the bias flag based on current indicators
        // This is already done in calculate_daily_indicators()
        // Just ensure the flag is set correctly
        multi_timeframe_data.daily_bullish_bias =
            multi_timeframe_data.daily_bullish_bias; // Already set in calculate_daily_indicators
    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error evaluating daily bias: " << e.what();
        log_message(error_stream.str(), "");
    }
}

void MultiTimeframeManager::evaluate_thirty_min_confirmation() {
    try {
        // Check if 30-minute timeframe confirms daily bias
        if (!multi_timeframe_data.daily_bullish_bias) {
            multi_timeframe_data.thirty_min_confirmation = false;
            return;
        }

        // Thirty-minute confirmation requires:
        // 1. EMA alignment with daily trend
        // 2. ADX showing sufficient strength
        // 3. Volume above threshold
        // 4. Spread within acceptable range

        bool ema_alignment = false;
        if (!multi_timeframe_data.thirty_min_bars.empty()) {
            double latest_close = multi_timeframe_data.thirty_min_bars.back().close_price;
            ema_alignment = latest_close > multi_timeframe_data.thirty_min_indicators.ema;
        }

        bool adx_strong = multi_timeframe_data.thirty_min_indicators.adx >= config.strategy.mth_ts_30min_adx_threshold;
        bool volume_ok = multi_timeframe_data.thirty_min_indicators.volume_ma >= config.strategy.mth_ts_30min_volume_multiplier_strict;
        bool spread_ok = multi_timeframe_data.thirty_min_indicators.spread_avg <= config.strategy.mth_ts_30min_avg_spread_threshold;

        multi_timeframe_data.thirty_min_confirmation = ema_alignment && adx_strong && volume_ok && spread_ok;

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error evaluating thirty-minute confirmation: " << e.what();
        log_message(error_stream.str(), "");
        multi_timeframe_data.thirty_min_confirmation = false;
    }
}

void MultiTimeframeManager::evaluate_minute_trigger() {
    try {
        // Check if 1-minute timeframe shows a valid trigger signal
        if (!multi_timeframe_data.thirty_min_confirmation) {
            multi_timeframe_data.minute_trigger_signal = false;
            return;
        }

        // One-minute trigger requires:
        // 1. Fast EMA above slow EMA (bullish crossover)
        // 2. RSI not overbought (below threshold)
        // 3. Volume confirmation
        // 4. Spread acceptable

        bool ema_crossover = false;
        if (!multi_timeframe_data.minute_bars.empty()) {
            // Simple check: current close > fast EMA and fast EMA > slow EMA approximation
            double latest_close = multi_timeframe_data.minute_bars.back().close_price;
            ema_crossover = latest_close > multi_timeframe_data.minute_indicators.ema;
        }

        bool rsi_ok = multi_timeframe_data.minute_indicators.rsi <= config.strategy.mth_ts_1min_rsi_threshold;
        bool volume_ok = multi_timeframe_data.minute_indicators.volume_ma >= config.strategy.mth_ts_1min_volume_multiplier;
        bool spread_ok = multi_timeframe_data.minute_indicators.spread_avg <= config.strategy.mth_ts_1min_spread_threshold;

        multi_timeframe_data.minute_trigger_signal = ema_crossover && rsi_ok && volume_ok && spread_ok;

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error evaluating minute trigger: " << e.what();
        log_message(error_stream.str(), "");
        multi_timeframe_data.minute_trigger_signal = false;
    }
}

void MultiTimeframeManager::evaluate_second_execution_readiness() {
    try {
        // Check if execution conditions are met at the 1-second level
        if (!multi_timeframe_data.minute_trigger_signal) {
            multi_timeframe_data.second_execution_ready = false;
            return;
        }

        // Second-level execution requires:
        // 1. Price momentum in the right direction
        // 2. Spread within tight threshold
        // 3. Recent price action confirms trend

        bool spread_tight = multi_timeframe_data.second_indicators.spread_avg <= config.strategy.mth_ts_1sec_spread_threshold;

        // Simple momentum check: recent bars showing upward movement
        bool momentum_up = false;
        if (multi_timeframe_data.second_bars.size() >= 3) {
            const auto& bars = multi_timeframe_data.second_bars;
            size_t n = bars.size();
            // Check if last few bars are trending up
            momentum_up = (bars[n-1].close_price > bars[n-2].close_price) &&
                         (bars[n-2].close_price > bars[n-3].close_price);
        }

        multi_timeframe_data.second_execution_ready = spread_tight && momentum_up;

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error evaluating second execution readiness: " << e.what();
        log_message(error_stream.str(), "");
        multi_timeframe_data.second_execution_ready = false;
    }
}

std::string MultiTimeframeManager::get_timeframe_start_timestamp(const std::string& current_timestamp) const {
    // This would implement logic to determine the start timestamp for a given timeframe
    // For now, return the current timestamp
    return current_timestamp;
}

std::string MultiTimeframeManager::format_timestamp(const std::chrono::system_clock::time_point& tp) const {
    // Polygon.io expects Unix timestamps in milliseconds for historical bars
    auto duration = tp.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return std::to_string(milliseconds);
}

// Data management helpers
std::chrono::system_clock::time_point MultiTimeframeManager::parse_timestamp(const std::string& timestamp_str) const {
    try {
        // Assume timestamp_str is Unix milliseconds
        long long milliseconds = std::stoll(timestamp_str);
        auto duration = std::chrono::milliseconds(milliseconds);
        return std::chrono::system_clock::time_point(duration);
    } catch (const std::exception& e) {
        log_message("Error parsing timestamp: " + std::string(e.what()), "");
        return std::chrono::system_clock::time_point();
    }
}

std::chrono::system_clock::time_point MultiTimeframeManager::get_utc_midnight(const std::chrono::system_clock::time_point& tp) const {
    // Get the time_t representation
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);

    // Convert to tm structure
    std::tm tm_val = *std::gmtime(&time_t_val);

    // Set time to midnight
    tm_val.tm_hour = 0;
    tm_val.tm_min = 0;
    tm_val.tm_sec = 0;

    // Convert back to time_point
    return std::chrono::system_clock::from_time_t(std::mktime(&tm_val));
}

// Utility methods
bool MultiTimeframeManager::is_timestamp_in_timeframe(const std::string& timestamp, MthTsTimeframe timeframe,
                                                      const std::string& reference_timestamp) const {
    try {
        auto ts = parse_timestamp(timestamp);
        auto ref_ts = parse_timestamp(reference_timestamp);

        switch (timeframe) {
            case MthTsTimeframe::SECOND_1:
                // 1-second timeframe: within the same second
                return std::chrono::duration_cast<std::chrono::seconds>(ts - ref_ts).count() == 0;

            case MthTsTimeframe::MINUTE_1:
                // 1-minute timeframe: within the same minute
                return std::chrono::duration_cast<std::chrono::minutes>(ts - ref_ts).count() == 0;

            case MthTsTimeframe::MINUTE_30:
                // 30-minute timeframe: within the same 30-minute window
                {
                    auto ref_minutes = std::chrono::duration_cast<std::chrono::minutes>(ref_ts.time_since_epoch()).count();
                    auto ts_minutes = std::chrono::duration_cast<std::chrono::minutes>(ts.time_since_epoch()).count();
                    return (ref_minutes / 30) == (ts_minutes / 30);
                }

            case MthTsTimeframe::DAILY:
                // Daily timeframe: same day
                return get_utc_midnight(ts) == get_utc_midnight(ref_ts);

            default:
                return false;
        }
    } catch (const std::exception& e) {
        log_message("Error in is_timestamp_in_timeframe: " + std::string(e.what()), "");
        return false;
    }
}

// Partial bar access methods
std::deque<MultiTimeframeBar> MultiTimeframeManager::get_bars_with_partial(MthTsTimeframe tf) const {
    std::deque<MultiTimeframeBar> result;

    switch (tf) {
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

// Propagation detection methods
double MultiTimeframeManager::get_propagation_score(MthTsTimeframe lower_tf) const {
    switch (lower_tf) {
        case MthTsTimeframe::MINUTE_1:
            return multi_timeframe_data.minute_to_thirty_min_propagation_score;
        case MthTsTimeframe::SECOND_1:
            return multi_timeframe_data.second_to_minute_propagation_score;
        default:
            return 0.0;
    }
}

// Incremental update helpers for partial bars
void MultiTimeframeManager::update_minute_with_new_second(const MultiTimeframeBar& second_bar) {
    try {
        std::string minute_start = get_timeframe_start_timestamp(second_bar.timestamp);

        // Check if we need to start a new minute bar
        if (current_minute_start_ts.empty() || !is_timestamp_in_timeframe(second_bar.timestamp, MthTsTimeframe::MINUTE_1, current_minute_start_ts)) {
            // Push completed minute bar if it exists
            if (!current_minute_start_ts.empty()) {
                multi_timeframe_data.minute_bars.push_back(current_minute_bar);
                maintain_deque_size(multi_timeframe_data.minute_bars, config.strategy.mth_ts_maintenance_1min_max);

                // Update thirty-minute with new minute
                update_thirty_min_with_new_minute(current_minute_bar);
            }

            // Initialize new minute bar
            current_minute_bar = second_bar;
            current_minute_start_ts = minute_start;
            current_minute_count = 1;
        } else {
            // Update existing minute bar
            current_minute_bar.high_price = std::max(current_minute_bar.high_price, second_bar.high_price);
            current_minute_bar.low_price = std::min(current_minute_bar.low_price, second_bar.low_price);
            current_minute_bar.close_price = second_bar.close_price;
            current_minute_bar.volume += second_bar.volume;

            // Running average for spread
            double total_spread = current_minute_bar.spread * current_minute_count;
            total_spread += second_bar.spread;
            current_minute_count++;
            current_minute_bar.spread = total_spread / current_minute_count;
        }
    } catch (const std::exception& e) {
        std::cerr << "MTH-TS: Failed to update minute with new second: " << e.what() << std::endl;
    }
}

void MultiTimeframeManager::update_thirty_min_with_new_minute(const MultiTimeframeBar& new_minute_bar) {
    try {
        std::string thirty_min_start = get_timeframe_start_timestamp(new_minute_bar.timestamp);

        // Check if we need to start a new 30-minute bar
        if (current_thirty_min_start_ts.empty() || !is_timestamp_in_timeframe(new_minute_bar.timestamp, MthTsTimeframe::MINUTE_30, current_thirty_min_start_ts)) {
            // Push completed 30-minute bar if it exists
            if (!current_thirty_min_start_ts.empty()) {
                multi_timeframe_data.thirty_min_bars.push_back(current_thirty_min_bar);
                maintain_deque_size(multi_timeframe_data.thirty_min_bars, config.strategy.mth_ts_maintenance_30min_max);

                // Update daily with new 30-minute
                update_daily_with_new_thirty_min(current_thirty_min_bar);
            }

            // Initialize new 30-minute bar
            current_thirty_min_bar = new_minute_bar;
            current_thirty_min_start_ts = thirty_min_start;
            current_thirty_min_count = 1;
        } else {
            // Update existing 30-minute bar
            current_thirty_min_bar.high_price = std::max(current_thirty_min_bar.high_price, new_minute_bar.high_price);
            current_thirty_min_bar.low_price = std::min(current_thirty_min_bar.low_price, new_minute_bar.low_price);
            current_thirty_min_bar.close_price = new_minute_bar.close_price;
            current_thirty_min_bar.volume += new_minute_bar.volume;

            // Running average for spread
            double total_spread = current_thirty_min_bar.spread * current_thirty_min_count;
            total_spread += new_minute_bar.spread;
            current_thirty_min_count++;
            current_thirty_min_bar.spread = total_spread / current_thirty_min_count;
        }
    } catch (const std::exception& e) {
        std::cerr << "MTH-TS: Failed to update thirty_min with new minute: " << e.what() << std::endl;
    }
}

void MultiTimeframeManager::update_daily_with_new_thirty_min(const MultiTimeframeBar& new_thirty_min_bar) {
    try {
        std::string daily_start = get_timeframe_start_timestamp(new_thirty_min_bar.timestamp);

        // Check if we need to start a new daily bar
        if (current_daily_start_ts.empty() || !is_timestamp_in_timeframe(new_thirty_min_bar.timestamp, MthTsTimeframe::DAILY, current_daily_start_ts)) {
            // Push completed daily bar if it exists
            if (!current_daily_start_ts.empty()) {
                multi_timeframe_data.daily_bars.push_back(current_daily_bar);
                maintain_deque_size(multi_timeframe_data.daily_bars, config.strategy.mth_ts_maintenance_daily_max);
            }

            // Initialize new daily bar
            current_daily_bar = new_thirty_min_bar;
            current_daily_start_ts = daily_start;
            current_daily_count = 1;
        } else {
            // Update existing daily bar
            current_daily_bar.high_price = std::max(current_daily_bar.high_price, new_thirty_min_bar.high_price);
            current_daily_bar.low_price = std::min(current_daily_bar.low_price, new_thirty_min_bar.low_price);
            current_daily_bar.close_price = new_thirty_min_bar.close_price;
            current_daily_bar.volume += new_thirty_min_bar.volume;

            // Running average for spread
            double total_spread = current_daily_bar.spread * current_daily_count;
            total_spread += new_thirty_min_bar.spread;
            current_daily_count++;
            current_daily_bar.spread = total_spread / current_daily_count;
        }
    } catch (const std::exception& e) {
        std::cerr << "MTH-TS: Failed to update daily with new thirty_min: " << e.what() << std::endl;
    }
}

// Propagation detection helpers
// Note: Simplified propagation scoring uses current indicators directly rather than historical trends

void MultiTimeframeManager::update_propagation_scores() {
    try {
        // Get bars with partial data for real-time analysis
        auto minute_bars_with_partial = get_bars_with_partial(MthTsTimeframe::MINUTE_1);
        auto thirty_min_bars_with_partial = get_bars_with_partial(MthTsTimeframe::MINUTE_30);

        // Calculate second-to-minute propagation score
        // Check if minute timeframe shows upward momentum from recent second bars
        double second_to_minute_score = 0.0;
        if (minute_bars_with_partial.size() >= 2) {
            const auto& current_minute = minute_bars_with_partial.back();
            const auto& prev_minute = minute_bars_with_partial[minute_bars_with_partial.size() - 2];

            // Momentum propagation: price and volume moving higher
            bool price_momentum = current_minute.close_price > prev_minute.close_price;
            bool volume_momentum = current_minute.volume > prev_minute.volume;
            // Use current EMA vs previous close as simple upward trend indicator
            bool ema_trend = multi_timeframe_data.minute_indicators.ema > prev_minute.close_price;

            if (price_momentum && volume_momentum && ema_trend) {
                second_to_minute_score = 0.8; // Strong propagation
            } else if (price_momentum || (volume_momentum && ema_trend)) {
                second_to_minute_score = 0.5; // Moderate propagation
            }
        }
        multi_timeframe_data.second_to_minute_propagation_score = second_to_minute_score;

        // Calculate minute-to-thirty-minute propagation score
        // Check if thirty-minute timeframe is showing signs of following minute momentum
        double minute_to_thirty_score = 0.0;
        if (thirty_min_bars_with_partial.size() >= 2) {
            const auto& current_thirty = thirty_min_bars_with_partial.back();
            const auto& prev_thirty = thirty_min_bars_with_partial[thirty_min_bars_with_partial.size() - 2];

            // Check for emerging bullishness in 30-min timeframe
            bool price_trend = current_thirty.close_price > prev_thirty.close_price;
            bool ema_trend = multi_timeframe_data.thirty_min_indicators.ema > prev_thirty.close_price;
            bool adx_bullish = multi_timeframe_data.thirty_min_indicators.adx > 20.0; // ADX above 20 indicates trending

            if (ema_trend && adx_bullish) {
                minute_to_thirty_score = 0.9; // Very strong upward propagation
            } else if (price_trend && ema_trend) {
                minute_to_thirty_score = 0.7; // Good propagation
            } else if (price_trend || ema_trend) {
                minute_to_thirty_score = 0.4; // Weak propagation
            }
        }
        multi_timeframe_data.minute_to_thirty_min_propagation_score = minute_to_thirty_score;

    } catch (const std::exception& e) {
        std::cerr << "MTH-TS: Error updating propagation scores: " << e.what() << std::endl;
    }
}

} // namespace Core
} // namespace AlpacaTrader
