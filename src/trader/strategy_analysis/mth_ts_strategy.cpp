#include "mth_ts_strategy.hpp"
#include "trader/market_data/multi_timeframe_manager.hpp"
#include "trader/strategy_analysis/indicators.hpp"
#include "logging/logs/trading_logs.hpp"
#include <algorithm>
#include <cmath>

namespace AlpacaTrader {
namespace Core {

MthTsStrategy::MthTsStrategy(const SystemConfig& config, API::ApiManager& api_manager, int current_position_quantity, const ProcessedData& processed_data)
    : config(config), api_manager(api_manager), current_position_quantity(current_position_quantity), processed_data(processed_data), multi_timeframe_manager(nullptr) {
    // Get the MultiTimeframeManager from the PolygonCryptoClient
    try {
        API::PolygonCryptoClient* polygon_client = api_manager.get_polygon_crypto_client();
        if (polygon_client) {
            multi_timeframe_manager = polygon_client->get_multi_timeframe_manager();
            if (!multi_timeframe_manager) {
                throw std::runtime_error("MultiTimeframeManager is null");
            }
        } else {
            throw std::runtime_error("PolygonCryptoClient is null");
        }
    } catch (const std::exception& e) {
        std::string error_msg = "MTH-TS: Failed to get MultiTimeframeManager: " + std::string(e.what());
        log_message(error_msg, "");
        throw std::runtime_error(error_msg);
    }
}

// TradingStrategy interface implementation
SignalDecision MthTsStrategy::evaluate_signals(const ProcessedData& /*processed_data*/, API::ApiManager* /*api_manager*/) {
    // For MTH-TS, we use the existing evaluation logic
    MthTsAnalysisResult result = evaluate_mth_ts_strategy();
    return result.signal_decision;
}

MthTsAnalysisResult MthTsStrategy::evaluate_mth_ts_strategy() {
    MthTsAnalysisResult result;

    try {
        // Check if MTH-TS is enabled
        if (!config.strategy.mth_ts_enabled) {
            return result;
        }

        // DEFENSIVE PROGRAMMING: Ensure MultiTimeframeManager is available
        if (!multi_timeframe_manager) {
            throw std::runtime_error("MultiTimeframeManager is not available for MTH-TS evaluation");
        }

        // Evaluate all timeframe levels in hierarchical order
        bool daily_bias = evaluate_daily_level();
        bool thirty_min_confirmation = evaluate_thirty_min_level();
        bool one_min_trigger = evaluate_one_min_level();
        bool one_sec_execution = evaluate_one_sec_level();

        // Update consecutive bar counters for bottom-up risk mitigation
        // TEMPORARILY DISABLED FOR TESTING
        /*
        if (api_manager.get_polygon_crypto_client()) {
            auto* mtf_manager = api_manager.get_polygon_crypto_client()->get_multi_timeframe_manager();
            if (mtf_manager) {
                auto& mtf_data = mtf_manager->get_multi_timeframe_data();

                // Update consecutive minute bars
                if (one_min_trigger) {
                    mtf_data.consecutive_minute_bars_aligned++;
                } else {
                    mtf_data.consecutive_minute_bars_aligned = 0;
                }

                // Update consecutive second bars
                if (one_sec_execution) {
                    mtf_data.consecutive_second_bars_aligned++;
                } else {
                    mtf_data.consecutive_second_bars_aligned = 0;
                }
            }
        }
        */

        // Store timeframe status for logging
        result.timeframe_status.daily_bias = daily_bias;
        result.timeframe_status.thirty_min_confirmation = thirty_min_confirmation;
        result.timeframe_status.one_min_trigger = one_min_trigger;
        result.timeframe_status.one_sec_execution = one_sec_execution;

        // MTH-TS Hybrid Logic: Bottom-up start with propagation confirmation
        // Start with lower timeframes (1-min and 1-sec) for early detection
        // Use propagation scores to assess upward momentum before full confirmation
        // Thirty-minute provides final confirmation with reduced strength for provisional signals

        // HYBRID EVALUATION: Check lower timeframes first with consecutive bar safeguards
        bool lower_timeframes_aligned = one_min_trigger && one_sec_execution;

        // Get consecutive bar counts for bottom-up risk mitigation
        // TEMPORARILY DISABLED FOR TESTING
        /*
        int consecutive_minute_bars = 0;
        int consecutive_second_bars = 0;

        if (api_manager.get_polygon_crypto_client()) {
            auto* mtf_manager = api_manager.get_polygon_crypto_client()->get_multi_timeframe_manager();
            if (mtf_manager) {
                const auto& mtf_data = mtf_manager->get_multi_timeframe_data();
                consecutive_minute_bars = mtf_data.consecutive_minute_bars_aligned;
                consecutive_second_bars = mtf_data.consecutive_second_bars_aligned;
            }
        }

        // Require minimum consecutive bars to prevent bottom-up whipsaws
        bool meets_consecutive_requirement = (one_min_trigger ? consecutive_minute_bars >= config.strategy.mth_ts_min_consecutive_min_bars : true) &&
                                           (one_sec_execution ? consecutive_second_bars >= 1 : true); // Less strict for seconds
        */

        if (lower_timeframes_aligned) { // && meets_consecutive_requirement) {
            // Compute propagation score for upward momentum assessment
            // TEMPORARILY DISABLE PROPAGATION SCORE TO TEST
            // double propagation_score = compute_propagation_score(one_min_trigger, one_sec_execution);

            if (true) { // propagation_score >= config.strategy.mth_ts_propagation_min_score) {
                result.signal_decision.buy = true;
                result.signal_decision.signal_strength = config.strategy.mth_ts_signal_strength_provisional;
                result.signal_decision.signal_reason = "MTH-TS: Lower timeframes aligned with upward propagation (provisional)";

                if (thirty_min_confirmation) {
                    result.signal_decision.signal_strength = config.strategy.mth_ts_signal_strength_full;
                    result.signal_decision.signal_reason = "MTH-TS: All timeframes aligned with propagation - FULL BUY signal";
                }
            } else if (thirty_min_confirmation) {
                result.signal_decision.buy = true;
                result.signal_decision.signal_strength = config.strategy.mth_ts_signal_strength_medium;
                result.signal_decision.signal_reason = "MTH-TS: Thirty-minute confirms lower timeframe signals";
            }
        }

        // If no buy signal, check for sell/reversal signals
        if (!result.signal_decision.buy) {
            result.signal_decision.signal_strength = 0.0;
            result.signal_decision.signal_reason = "MTH-TS: Insufficient alignment or propagation";

            // Check for reversal signals
            bool reversal_signal = detect_reversal_signal();
            bool position_is_open = is_position_open();

            if (reversal_signal && position_is_open) {
                result.signal_decision.sell = true;
                result.signal_decision.signal_reason = "MTH-TS: Reversal signal detected - CLOSE position";
            } else {
                result.signal_decision.sell = false;
            }
        }

        return result;

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error evaluating MTH-TS strategy: " << e.what();
        log_message(error_stream.str(), "");
        return result;
    }
}

// Hybrid evaluation helpers
double MthTsStrategy::compute_propagation_score(bool one_min_aligned, bool one_sec_aligned) {
    try {
        // Get propagation scores from multi-timeframe manager
        double minute_to_thirty_score = 0.0;
        double second_to_minute_score = 0.0;

        if (api_manager.get_polygon_crypto_client()) {
            auto* mtf_manager = api_manager.get_polygon_crypto_client()->get_multi_timeframe_manager();
            if (mtf_manager) {
                minute_to_thirty_score = mtf_manager->get_propagation_score(MthTsTimeframe::MINUTE_1);
                second_to_minute_score = mtf_manager->get_propagation_score(MthTsTimeframe::SECOND_1);
            }
        }

        // Combine scores based on alignment
        double combined_score = 0.0;

        if (one_min_aligned && one_sec_aligned) {
            combined_score = (minute_to_thirty_score * config.strategy.mth_ts_propagation_weight_minute_to_thirty) + 
                           (second_to_minute_score * config.strategy.mth_ts_propagation_weight_second_to_minute);
        } else if (one_min_aligned) {
            combined_score = minute_to_thirty_score * config.strategy.mth_ts_propagation_weight_minute_only;
        } else if (one_sec_aligned) {
            combined_score = second_to_minute_score * config.strategy.mth_ts_propagation_weight_second_only;
        }

        // Ensure score is within valid range
        combined_score = std::max(0.0, std::min(1.0, combined_score));

        return combined_score;

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error computing propagation score: " << e.what();
        log_message(error_stream.str(), "");
        return 0.0;
    }
}

bool MthTsStrategy::evaluate_daily_level() {
    try {
        // Check if multi-timeframe manager is available
        if (!multi_timeframe_manager) {
            return false;
        }

        // Use bars with partial to include unfinished data
        auto daily_bars_with_partial = multi_timeframe_manager->get_bars_with_partial(MthTsTimeframe::DAILY);

        // Need sufficient daily bars for meaningful analysis
        if (static_cast<int>(daily_bars_with_partial.size()) < config.strategy.mth_ts_min_daily_bars) return false;

        // Need at least 2 bars for technical analysis comparison
        if (daily_bars_with_partial.size() < 2) return false;

        const auto& current_bar = daily_bars_with_partial.back();
        const auto& previous_bar = daily_bars_with_partial[daily_bars_with_partial.size() - 2];

        double daily_ema = calculate_ema(daily_bars_with_partial, config.strategy.mth_ts_daily_ema_period);
        double daily_adx = calculate_adx(daily_bars_with_partial, config.strategy.mth_ts_daily_adx_period);
        double daily_atr = calculate_atr(daily_bars_with_partial, config.strategy.mth_ts_atr_period);
        double daily_spread_avg = calculate_average_spread(daily_bars_with_partial, config.strategy.mth_ts_daily_spread_lookback_bars);

        auto& mtf_data = multi_timeframe_manager->get_multi_timeframe_data();
        mtf_data.daily_indicators.ema = daily_ema;
        mtf_data.daily_indicators.adx = daily_adx;
        mtf_data.daily_indicators.atr = daily_atr;
        mtf_data.daily_indicators.spread_avg = daily_spread_avg;

        auto analysis_result = perform_comprehensive_technical_analysis(current_bar, previous_bar, daily_atr);

        bool ema_alignment = current_bar.close_price > daily_ema;

        return ema_alignment && analysis_result.consolidated_ready;

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error evaluating daily level: " << e.what();
        log_message(error_stream.str(), "");
        return false;
    }
}

bool MthTsStrategy::evaluate_thirty_min_level() {
    try {
        // Check if multi-timeframe manager is available
        if (!multi_timeframe_manager) {
            return false;
        }

        // Use bars with partial to include unfinished data
        auto thirty_min_bars_with_partial = multi_timeframe_manager->get_bars_with_partial(MthTsTimeframe::MINUTE_30);

        // Need sufficient 30-min bars for meaningful analysis
        if (static_cast<int>(thirty_min_bars_with_partial.size()) < config.strategy.mth_ts_min_30min_bars) return false;

        // Need at least 2 bars for technical analysis comparison
        if (thirty_min_bars_with_partial.size() < 2) return false;

        const auto& current_bar = thirty_min_bars_with_partial.back();
        const auto& previous_bar = thirty_min_bars_with_partial[thirty_min_bars_with_partial.size() - 2];

        double thirty_min_ema = calculate_ema(thirty_min_bars_with_partial, config.strategy.mth_ts_30min_ema_period);
        double thirty_min_adx = calculate_adx(thirty_min_bars_with_partial, config.strategy.mth_ts_30min_adx_period);
        double thirty_min_atr = calculate_atr(thirty_min_bars_with_partial, config.strategy.mth_ts_atr_period);
        double thirty_min_volume_ma = calculate_volume_ma(thirty_min_bars_with_partial, config.strategy.mth_ts_30min_volume_ma_period);
        double thirty_min_spread_avg = calculate_average_spread(thirty_min_bars_with_partial, config.strategy.mth_ts_30min_spread_lookback_bars);

        auto& mtf_data = multi_timeframe_manager->get_multi_timeframe_data();
        mtf_data.thirty_min_indicators.ema = thirty_min_ema;
        mtf_data.thirty_min_indicators.adx = thirty_min_adx;
        mtf_data.thirty_min_indicators.atr = thirty_min_atr;
        mtf_data.thirty_min_indicators.volume_ma = thirty_min_volume_ma;
        mtf_data.thirty_min_indicators.spread_avg = thirty_min_spread_avg;

        auto analysis_result = perform_comprehensive_technical_analysis(current_bar, previous_bar, thirty_min_atr);

        bool ema_alignment = current_bar.close_price > thirty_min_ema;
        bool adx_strong = thirty_min_adx >= config.strategy.mth_ts_30min_adx_threshold;
        bool spread_ok = thirty_min_spread_avg <= config.strategy.mth_ts_30min_avg_spread_threshold;

        return ema_alignment && adx_strong && spread_ok && analysis_result.consolidated_ready;

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error evaluating 30-min level: " << e.what();
        log_message(error_stream.str(), "");
        return false;
    }
}

bool MthTsStrategy::evaluate_one_min_level() {
    try {
        // Check if multi-timeframe manager is available
        if (!multi_timeframe_manager) {
            return false;
        }

        // Use bars with partial to include unfinished data
        auto minute_bars_with_partial = multi_timeframe_manager->get_bars_with_partial(MthTsTimeframe::MINUTE_1);

        // Need sufficient 1-min bars for meaningful analysis
        if (static_cast<int>(minute_bars_with_partial.size()) < config.strategy.mth_ts_min_1min_bars) return false;

        // Need at least 2 bars for technical analysis comparison
        if (minute_bars_with_partial.size() < 2) return false;

        const auto& current_bar = minute_bars_with_partial.back();
        const auto& previous_bar = minute_bars_with_partial[minute_bars_with_partial.size() - 2];

        double minute_ema = calculate_ema(minute_bars_with_partial, config.strategy.mth_ts_1min_fast_ema_period);
        double minute_rsi = calculate_rsi(minute_bars_with_partial, config.strategy.mth_ts_1min_rsi_period);
        double minute_atr = calculate_atr(minute_bars_with_partial, config.strategy.mth_ts_atr_period);
        double minute_volume_ma = calculate_volume_ma(minute_bars_with_partial, config.strategy.mth_ts_1min_volume_ma_period);
        double minute_spread_avg = calculate_average_spread(minute_bars_with_partial, config.strategy.mth_ts_1min_spread_lookback_bars);

        auto& mtf_data = multi_timeframe_manager->get_multi_timeframe_data();
        mtf_data.minute_indicators.ema = minute_ema;
        mtf_data.minute_indicators.rsi = minute_rsi;
        mtf_data.minute_indicators.atr = minute_atr;
        mtf_data.minute_indicators.volume_ma = minute_volume_ma;
        mtf_data.minute_indicators.spread_avg = minute_spread_avg;

        auto analysis_result = perform_comprehensive_technical_analysis(current_bar, previous_bar, minute_atr);

        bool ema_crossover = current_bar.close_price > minute_ema;
        bool rsi_ok = minute_rsi >= config.strategy.mth_ts_1min_rsi_threshold &&
                      minute_rsi <= config.strategy.mth_ts_1min_rsi_threshold_high;
        bool volume_ok = minute_volume_ma >= config.strategy.mth_ts_1min_volume_multiplier;
        bool spread_ok = minute_spread_avg <= config.strategy.mth_ts_1min_spread_threshold;

        return ema_crossover && rsi_ok && volume_ok && spread_ok && analysis_result.consolidated_ready;

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error evaluating 1-min level: " << e.what();
        log_message(error_stream.str(), "");
        return false;
    }
}

MthTsStrategy::TimeframeAnalysisResult MthTsStrategy::perform_comprehensive_technical_analysis(
    const MultiTimeframeBar& current_bar,
    const MultiTimeframeBar& previous_bar,
    double atr_value) const {

    TimeframeAnalysisResult result = {};

    try {
        // ATR filter - ensures sufficient volatility for trading
        result.atr_filter_pass = atr_value > config.strategy.atr_absolute_minimum_threshold;

        // Volume filter - ensures sufficient liquidity/interest
        result.volume_filter_pass = (previous_bar.volume > 0.0) ?
            (current_bar.volume / previous_bar.volume) >= config.strategy.entry_signal_volume_multiplier : false;

        // Doji filter (no doji = pass) - avoids indecisive candles
        result.doji_filter_pass = true; // Default to pass, only fail if doji detected
        if (current_bar.high_price > 0.0 && current_bar.low_price > 0.0) {
            double body_size = std::abs(current_bar.close_price - current_bar.open_price);
            double total_range = current_bar.high_price - current_bar.low_price;
            if (total_range > 0.0) {
                double body_percentage = body_size / total_range;
                result.doji_filter_pass = body_percentage >= config.strategy.doji_candlestick_body_size_threshold_percentage;
            }
        }

        // Price change analysis (momentum) - measures short-term price movement
        double price_change_percentage = (previous_bar.close_price > 0.0) ?
            ((current_bar.close_price - previous_bar.close_price) / previous_bar.close_price) * 100.0 : 0.0;

        // Volume change analysis - measures volume momentum
        double volume_change_percentage = (previous_bar.volume > 0.0) ?
            ((current_bar.volume - previous_bar.volume) / previous_bar.volume) * 100.0 : 0.0;

        // For crypto: amplify volume changes for better sensitivity
        if (config.strategy.is_crypto_asset) {
            volume_change_percentage *= config.strategy.crypto_volume_change_amplification_factor;
        }

        // Volatility analysis (ATR-based) - measures market volatility
        double volatility_percentage = (previous_bar.close_price > 0.0) ?
            (atr_value / previous_bar.close_price) * config.strategy.percentage_calculation_multiplier : 0.0;

        // Basic price pattern analysis - identifies bullish candle patterns
        result.basic_buy_pattern = config.strategy.buy_signals_allow_equal_close ?
            (current_bar.close_price >= current_bar.open_price) :
            (current_bar.close_price > current_bar.open_price);

        // Higher high condition - requires price to make new highs
        result.buy_high_condition = config.strategy.buy_signals_require_higher_high ?
            (current_bar.high_price > previous_bar.high_price) : true;

        // Higher low condition - requires price to make higher lows
        result.buy_low_condition = config.strategy.buy_signals_require_higher_low ?
            (current_bar.low_price >= previous_bar.low_price) : true;

        // Momentum confirmation for buy signals - ensures upward momentum
        result.momentum_buy_signal = price_change_percentage > config.strategy.minimum_price_change_percentage_for_momentum;
        result.volume_buy_confirmation = volume_change_percentage > config.strategy.minimum_volume_increase_percentage_for_buy_signals;
        result.volatility_buy_confirmation = volatility_percentage > config.strategy.minimum_volatility_percentage_for_buy_signals;

        // CONSOLIDATED DECISION: All technical conditions must align
        result.consolidated_ready = result.atr_filter_pass &&
                                   result.volume_filter_pass &&
                                   result.doji_filter_pass &&
                                   result.basic_buy_pattern &&
                                   result.buy_high_condition &&
                                   result.buy_low_condition &&
                                   result.momentum_buy_signal &&
                                   result.volume_buy_confirmation &&
                                   result.volatility_buy_confirmation;

        return result;

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error in comprehensive technical analysis: " << e.what();
        log_message(error_stream.str(), "");
        return result; // Return default (all false) result
    }
}

bool MthTsStrategy::evaluate_one_sec_level() {
    try {
        // Check if multi-timeframe manager is available
        if (!multi_timeframe_manager) {
            return false;
        }

        const auto& mtf_data = get_multi_timeframe_data();

        // Use bars with partial to include unfinished data
        auto second_bars_with_partial = multi_timeframe_manager->get_bars_with_partial(MthTsTimeframe::SECOND_1);

        // Need at least some 1-sec bars
        if (static_cast<int>(second_bars_with_partial.size()) < config.strategy.mth_ts_min_1sec_bars) return false;

        // INTEGRATED ANALYSIS: Combine MTH-TS execution readiness with comprehensive technical analysis
        // Start with MTH-TS execution readiness (spread + momentum check)
        bool mth_ts_execution_ready = mtf_data.second_execution_ready;

        // Need at least 2 bars for technical analysis comparison
        if (second_bars_with_partial.size() < 2) return false;

        const auto& current_bar = second_bars_with_partial.back();
        const auto& previous_bar = second_bars_with_partial[second_bars_with_partial.size() - 2];

        double second_atr = calculate_atr(second_bars_with_partial, config.strategy.mth_ts_atr_period);
        double second_spread_avg = calculate_average_spread(second_bars_with_partial, config.strategy.mth_ts_1sec_spread_lookback_bars);
        double second_volume_ma = calculate_volume_ma(second_bars_with_partial, config.strategy.mth_ts_1sec_volume_ma_period);

        auto& mtf_data_mutable = multi_timeframe_manager->get_multi_timeframe_data();
        mtf_data_mutable.second_indicators.atr = second_atr;
        mtf_data_mutable.second_indicators.spread_avg = second_spread_avg;
        mtf_data_mutable.second_indicators.volume_ma = second_volume_ma;

        auto analysis_result = perform_comprehensive_technical_analysis(
            current_bar, previous_bar,
            processed_data.atr);

        bool consolidated_ready = mth_ts_execution_ready && analysis_result.consolidated_ready;

        return consolidated_ready;

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error evaluating 1-sec level: " << e.what();
        log_message(error_stream.str(), "");
        return false;
    }
}

OrderExecutionResult MthTsStrategy::execute_mth_ts_order(MthTsStrategy::OrderSide order_side) {
    OrderExecutionResult execution_result;

    try {
        // Only buy orders for MTH-TS (crypto cannot be shorted)
        if (order_side != MthTsStrategy::OrderSide::Buy) {
            execution_result.execution_status = "INVALID_ORDER_TYPE";
            execution_result.error_message = "MTH-TS only supports buy orders for crypto assets";
            return execution_result;
        }

        // Check if already have a position
        if (is_position_open()) {
            execution_result.execution_status = "POSITION_ALREADY_EXISTS";
            execution_result.error_message = "Cannot execute order - position already exists";
            return execution_result;
        }

        double current_price = get_current_price();
        if (current_price <= 0.0) {
            execution_result.execution_status = "INVALID_PRICE";
            execution_result.error_message = "Cannot execute order - invalid current price";
            return execution_result;
        }

        // Calculate position size based on notional value
        double quantity = config.strategy.mth_ts_position_notional / current_price;

        // Ensure minimum quantity for crypto
        if (quantity < config.strategy.mth_ts_min_crypto_quantity) {
            quantity = config.strategy.mth_ts_min_crypto_quantity;
        }

        // Place market buy order
        place_market_order(MthTsStrategy::OrderSide::Buy, quantity);

        // Calculate take profit and stop loss levels
        double take_profit_price = current_price * (1.0 + config.strategy.mth_ts_take_profit_percentage);
        double stop_loss_price = current_price * (1.0 - config.strategy.mth_ts_stop_loss_percentage);

        // Place take profit limit order (sell)
        place_limit_order(MthTsStrategy::OrderSide::Sell, quantity, take_profit_price);

        // Place stop loss stop-limit order (sell)
        double stop_limit_price = stop_loss_price * config.strategy.mth_ts_stop_limit_multiplier;
        place_stop_limit_order(MthTsStrategy::OrderSide::Sell, quantity, stop_loss_price, stop_limit_price);

        // Set execution result
        execution_result.order_successful = true;
        execution_result.execution_status = "ORDERS_PLACED";
        execution_result.executed_quantity = quantity;
        execution_result.executed_price = current_price;

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error executing MTH-TS order: " << e.what();
        log_message(error_stream.str(), "");
        execution_result.execution_status = "EXECUTION_FAILED";
        execution_result.error_message = std::string("Exception during order execution: ") + e.what();
    }

    return execution_result;
}

PositionClosureResult MthTsStrategy::check_and_close_positions() {
    PositionClosureResult closure_result;

    try {
        if (!is_position_open()) {
            closure_result.closure_successful = true;
            closure_result.closure_reason = "NO_POSITION_OPEN";
            return closure_result;
        }

        // Check for reversal signal
        if (detect_reversal_signal()) {
            close_all_positions();
            closure_result.closure_successful = true;
            closure_result.closure_reason = "REVERSAL_SIGNAL_DETECTED";
        } else {
            closure_result.closure_successful = true;
            closure_result.closure_reason = "NO_CLOSURE_NEEDED";
        }

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error checking position closure: " << e.what();
        log_message(error_stream.str(), "");
        closure_result.closure_successful = false;
        closure_result.error_message = std::string("Exception during position closure check: ") + e.what();
    }

    return closure_result;
}

bool MthTsStrategy::detect_reversal_signal() {
    try {
        if (!config.strategy.mth_ts_reversal_detection_enabled) return false;

        const auto& mtf_data = get_multi_timeframe_data();

        // Check if we have enough data
        if (static_cast<int>(mtf_data.second_bars.size()) < config.strategy.mth_ts_reversal_min_bars) return false;

        // Simple reversal detection: significant spread increase + momentum change

        // Check for high spread (potential volatility/reversal)
        bool high_spread = mtf_data.second_indicators.spread_avg > config.strategy.mth_ts_reversal_spread_threshold;

        // Check for momentum reversal (last few bars show downward movement)
        bool momentum_reversal = false;
        if (static_cast<int>(mtf_data.second_bars.size()) >= config.strategy.mth_ts_reversal_momentum_bars) {
            const auto& bars = mtf_data.second_bars;
            size_t n = bars.size();
            // Check if last bar is lower than previous bars (reversal pattern)
            momentum_reversal = (bars[n-1].close_price < bars[n-2].close_price) &&
                               (bars[n-2].close_price < bars[n-3].close_price);
        }

        return high_spread && momentum_reversal;

    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Error detecting reversal: " << e.what();
        log_message(error_stream.str(), "");
        return false;
    }
}

const MultiTimeframeData& MthTsStrategy::get_multi_timeframe_data() const {
    auto* polygon_client = api_manager.get_polygon_crypto_client();
    if (!polygon_client) {
        throw std::runtime_error("Polygon client not available");
    }

    auto* mtf_manager = polygon_client->get_multi_timeframe_manager();
    if (!mtf_manager) {
        throw std::runtime_error("Multi-timeframe manager not initialized");
    }

    return mtf_manager->get_multi_timeframe_data();
}

MultiTimeframeManager* MthTsStrategy::get_multi_timeframe_manager() const {
    auto* polygon_client = api_manager.get_polygon_crypto_client();
    if (!polygon_client) {
        return nullptr;
    }

    return polygon_client->get_multi_timeframe_manager();
}

// Private helper methods
double MthTsStrategy::get_current_price() const {
    try {
        return api_manager.get_current_price(config.strategy.symbol);
    } catch (const std::exception& e) {
        log_message("Error getting current price: " + std::string(e.what()), "");
        return 0.0;
    }
}

int MthTsStrategy::get_current_position() const {
    return current_position_quantity;
}

bool MthTsStrategy::is_position_open() const {
    return get_current_position() != 0;
}

void MthTsStrategy::place_market_order(MthTsStrategy::OrderSide /*side*/, double /*quantity*/) {
    try {
        // This would need to be implemented to place orders via the API manager
    } catch (const std::exception& /*e*/) {
        // Order placement failed
    }
}

void MthTsStrategy::place_limit_order(MthTsStrategy::OrderSide /*side*/, double /*quantity*/, double /*price*/) {
    try {
        // This would need to be implemented to place orders via the API manager
    } catch (const std::exception& /*e*/) {
        // Order placement failed
    }
}

void MthTsStrategy::place_stop_limit_order(MthTsStrategy::OrderSide /*side*/, double /*quantity*/, double /*stop_price*/, double /*limit_price*/) {
    try {
        // This would need to be implemented to place orders via the API manager
    } catch (const std::exception& /*e*/) {
        // Order placement failed
    }
}

void MthTsStrategy::close_all_positions() {
    try {
        // This would need to be implemented to close positions via the API manager
    } catch (const std::exception& e) {
        log_message("Error closing positions: " + std::string(e.what()), "");
    }
}

// Free functions for compatibility with trading_logic.cpp
SignalDecision detect_trading_signals(const ProcessedData& processed_data_input, const SystemConfig& system_config, API::ApiManager* api_manager) {
    // MTH-TS strategy for crypto assets
    if (system_config.strategy.mth_ts_enabled && system_config.strategy.is_crypto_asset && api_manager) {
        try {
            // Create and evaluate MTH-TS strategy directly
            MthTsStrategy mth_ts_strategy(system_config, *api_manager, processed_data_input.pos_details.position_quantity, processed_data_input);
            MthTsAnalysisResult mth_ts_result = mth_ts_strategy.evaluate_mth_ts_strategy();

            // Log MTH-TS analysis using consolidated table
            Logging::TradingLogs::log_mth_ts_strategy_header();

            // Log consolidated analysis for all timeframes
            API::PolygonCryptoClient* polygon_client = api_manager->get_polygon_crypto_client();
            if (polygon_client) {
                auto* mtf_manager = polygon_client->get_multi_timeframe_manager();
                if (mtf_manager) {
                    Logging::TradingLogs::log_mth_ts_consolidated_analysis(mtf_manager->get_multi_timeframe_data(), mth_ts_result.timeframe_status, mth_ts_result.signal_decision, processed_data_input);
                }
            }

            Logging::TradingLogs::log_mth_ts_analysis_complete();

            return mth_ts_result.signal_decision;
        } catch (const std::exception& e) {
            std::string error_msg = "MTH-TS strategy evaluation failed - " + std::string(e.what());
            log_message(error_msg, "");
            throw std::runtime_error(error_msg);
        }
    }

    if (system_config.strategy.is_crypto_asset) {
        std::string error_msg = "Crypto asset detected but MTH-TS strategy not enabled. Crypto trading requires MTH-TS strategy.";
        log_message(error_msg, "");
        throw std::runtime_error(error_msg);
    }

    // For non-crypto, return no signal (could implement basic strategy later)
    SignalDecision no_signal;
    return no_signal;
}

FilterResult evaluate_trading_filters(const ProcessedData& processed_data_input, const SystemConfig& system_config) {
    FilterResult filter_result_output = {};

    // ATR filter
    filter_result_output.atr_pass = processed_data_input.atr > system_config.strategy.atr_absolute_minimum_threshold;

    // Volume filter
    filter_result_output.vol_pass = processed_data_input.avg_vol > 0.0;

    // Doji filter
    filter_result_output.doji_pass = !processed_data_input.is_doji;

    // Calculate ratios for logging
    filter_result_output.atr_ratio = processed_data_input.atr;
    filter_result_output.vol_ratio = processed_data_input.avg_vol;

    // Overall result
    filter_result_output.all_pass = filter_result_output.atr_pass &&
                                   filter_result_output.vol_pass &&
                                   filter_result_output.doji_pass;

    return filter_result_output;
}

std::pair<PositionSizing, SignalDecision> process_position_sizing(const PositionSizingProcessRequest& request) {
    PositionSizing position_sizing_result = {};
    SignalDecision signal_decision = {}; // Default no signal

    try {
        // Hybrid position sizing - adjust risk based on signal strength
        // Provisional signals (lower strength) use reduced position sizes
        double base_risk_pct = 0.01; // 1% base risk
        double adjusted_risk_pct = base_risk_pct * request.signal_strength; // Scale by signal strength

        double risk_amount = request.account_equity * adjusted_risk_pct;
        double current_price = request.processed_data.curr.close_price;

        if (current_price > 0.0) {
            position_sizing_result.quantity = risk_amount / current_price;
        }

        // Cap at reasonable limits
        if (position_sizing_result.quantity > 1000.0) {
            position_sizing_result.quantity = 1000.0;
        }

        // Basic position sizing fields
        position_sizing_result.risk_amount = risk_amount;
        position_sizing_result.size_multiplier = 1.0;
        position_sizing_result.risk_based_qty = position_sizing_result.quantity;
        position_sizing_result.exposure_based_qty = position_sizing_result.quantity;
        position_sizing_result.max_value_qty = position_sizing_result.quantity;
        position_sizing_result.buying_power_qty = request.available_buying_power / current_price;

    } catch (const std::exception& e) {
        log_message("Error in position sizing: " + std::string(e.what()), "");
        position_sizing_result.quantity = 0.0;
    }

    return {position_sizing_result, signal_decision};
}

ExitTargets compute_exit_targets(const ExitTargetsRequest& request) {
    ExitTargets result = {};

    try {
        // Simple exit targets with fixed percentages
        double profit_pct = 0.02;  // 2% profit target
        double loss_pct = 0.01;    // 1% stop loss

        if (request.position_side == "buy") {
            // For long positions
            result.take_profit = request.entry_price * (1.0 + profit_pct);
            result.stop_loss = request.entry_price * (1.0 - loss_pct);
        } else if (request.position_side == "sell") {
            // For short positions
            result.take_profit = request.entry_price * (1.0 - profit_pct);
            result.stop_loss = request.entry_price * (1.0 + loss_pct);
        } else {
            // Default
            result.take_profit = request.entry_price * 1.02; // 2% profit
            result.stop_loss = request.entry_price * 0.98;   // 2% loss
        }

    } catch (const std::exception& e) {
        log_message("Error computing exit targets: " + std::string(e.what()), "");
        result.take_profit = request.entry_price;
        result.stop_loss = request.entry_price;
    }

    return result;
}

} // namespace Core
} // namespace AlpacaTrader
