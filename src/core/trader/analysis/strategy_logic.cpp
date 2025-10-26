#include "strategy_logic.hpp"
#include "indicators.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/logging/async_logger.hpp"
#include "core/utils/time_utils.hpp"
#include <cmath>
#include <climits>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

// Helper function to detect doji pattern
bool detect_doji_pattern(double open, double high, double low, double close, double doji_threshold) {
    double body_size = std::abs(close - open);
    double total_range = high - low;
    
    if (total_range == 0.0) return false; // Avoid division by zero
    
    // Doji if body is less than threshold% of total range
    return (body_size / total_range) < doji_threshold;
}

SignalDecision detect_trading_signals(const ProcessedData& data, const SystemConfig& config) {
    SignalDecision decision;
    
    // Calculate momentum indicators for better signal detection
    double price_change = data.curr.c - data.prev.c;
    double price_change_pct = (data.prev.c > 0.0) ? (price_change / data.prev.c) * config.strategy.percentage_calculation_multiplier : 0.0;
    
    // Calculate volume momentum with crypto-specific handling
    double volume_change = data.curr.v - data.prev.v;
    double volume_change_pct = (data.prev.v > 0) ? (volume_change / data.prev.v) * config.strategy.percentage_calculation_multiplier : 0.0;
    
    // For crypto: normalize volume change percentage to be more sensitive to small changes
    if (config.strategy.is_crypto_asset) {
        // Crypto volumes are much smaller, so amplify the percentage for better signal detection
        volume_change_pct *= config.strategy.crypto_volume_change_amplification_factor;
    }
    
    // Calculate volatility (ATR-based)
    double volatility_pct = (data.prev.c > 0.0) ? (data.atr / data.prev.c) * config.strategy.percentage_calculation_multiplier : 0.0;
    
    // Enhanced BUY signal conditions with momentum confirmation
    bool basic_buy_close = config.strategy.buy_signals_allow_equal_close ? 
                          (data.curr.c >= data.curr.o) : 
                          (data.curr.c > data.curr.o);
    
    bool buy_high_condition = config.strategy.buy_signals_require_higher_high ? 
                             (data.curr.h > data.prev.h) : 
                             true;
    
    bool buy_low_condition = config.strategy.buy_signals_require_higher_low ? 
                            (data.curr.l >= data.prev.l) : 
                            true;
    
    // Momentum-based buy confirmation (configurable thresholds)
    bool momentum_buy = price_change_pct > config.strategy.minimum_price_change_percentage_for_momentum;
    bool volume_confirmation = volume_change_pct > config.strategy.minimum_volume_increase_percentage_for_buy_signals;
    bool volatility_adequate = volatility_pct > config.strategy.minimum_volatility_percentage_for_buy_signals;
    
    // Calculate signal strength and reasoning
    double buy_strength = 0.0;
    std::string buy_reason = "";
    
    if (basic_buy_close && buy_high_condition && buy_low_condition) {
        buy_strength += config.strategy.basic_price_pattern_weight; // Basic pattern strength
        buy_reason += "Basic pattern OK; ";
        
        if (momentum_buy) {
            buy_strength += config.strategy.momentum_indicator_weight; // Momentum strength
            buy_reason += "Momentum OK; ";
        } else {
            buy_reason += "No momentum; ";
        }
        
        if (volume_confirmation) {
            buy_strength += config.strategy.volume_analysis_weight; // Volume confirmation
            buy_reason += "Volume OK; ";
        } else {
            buy_reason += "Low volume; ";
        }
        
        if (volatility_adequate) {
            buy_strength += config.strategy.volatility_analysis_weight; // Volatility confirmation
            buy_reason += "Volatility OK; ";
        } else {
            buy_reason += "Low volatility; ";
        }
    } else {
        buy_reason = "Basic pattern failed";
    }
    
    // Set buy signal if strength is above threshold
    decision.buy = buy_strength >= config.strategy.minimum_signal_strength_threshold;
    decision.signal_strength = buy_strength;
    decision.signal_reason = buy_reason;
    
    // Enhanced SELL signal conditions with momentum confirmation
    bool basic_sell_close = config.strategy.sell_signals_allow_equal_close ? 
                           (data.curr.c <= data.curr.o) : 
                           (data.curr.c < data.curr.o);
    
    bool sell_low_condition = config.strategy.sell_signals_require_lower_low ? 
                             (data.curr.l < data.prev.l) : 
                             true;
    
    bool sell_high_condition = config.strategy.sell_signals_require_lower_high ? 
                              (data.curr.h <= data.prev.h) : 
                              true;
    
    // Momentum-based sell confirmation (configurable thresholds)
    bool momentum_sell = price_change_pct < -config.strategy.minimum_price_change_percentage_for_momentum;
    bool volume_sell_confirmation = volume_change_pct > config.strategy.minimum_volume_increase_percentage_for_sell_signals;
    bool volatility_sell_adequate = volatility_pct > config.strategy.minimum_volatility_percentage_for_sell_signals;
    
    // Calculate sell signal strength and reasoning
    double sell_strength = 0.0;
    std::string sell_reason = "";
    
    if (basic_sell_close && sell_low_condition && sell_high_condition) {
        sell_strength += config.strategy.basic_price_pattern_weight; // Basic pattern strength
        sell_reason += "Basic pattern OK; ";
        
        if (momentum_sell) {
            sell_strength += config.strategy.momentum_indicator_weight; // Momentum strength
            sell_reason += "Momentum OK; ";
        } else {
            sell_reason += "No momentum; ";
        }
        
        if (volume_sell_confirmation) {
            sell_strength += config.strategy.volume_analysis_weight; // Volume confirmation
            sell_reason += "Volume OK; ";
        } else {
            sell_reason += "Low volume; ";
        }
        
        if (volatility_sell_adequate) {
            sell_strength += config.strategy.volatility_analysis_weight; // Volatility confirmation
            sell_reason += "Volatility OK; ";
        } else {
            sell_reason += "Low volatility; ";
        }
    } else {
        sell_reason = "Basic pattern failed";
    }
    
    // Set sell signal if strength is above threshold
    decision.sell = sell_strength >= config.strategy.minimum_signal_strength_threshold;
    
    // Update signal strength and reason (use the stronger signal)
    if (sell_strength > decision.signal_strength) {
        decision.signal_strength = sell_strength;
        decision.signal_reason = sell_reason;
    }
    
    return decision;
}

FilterResult evaluate_trading_filters(const ProcessedData& data, const SystemConfig& config) {
    FilterResult result;
    
    // ATR filter: use absolute threshold if enabled, otherwise use relative threshold
    if (config.strategy.use_absolute_atr_threshold) {
        result.atr_pass = data.atr > config.strategy.atr_absolute_minimum_threshold;
    } else {
        result.atr_pass = data.atr > config.strategy.entry_signal_atr_multiplier * data.avg_atr;
    }
    
    // Crypto-specific volume filtering: use different thresholds for crypto vs stocks
    if (config.strategy.is_crypto_asset) {
        // For crypto: use crypto-specific volume multiplier for fractional volumes
        double crypto_threshold = config.strategy.crypto_volume_multiplier * data.avg_vol;
        result.vol_pass = data.curr.v > crypto_threshold;
        
    } else {
        // For stocks: use original volume multiplier
        result.vol_pass = data.curr.v > config.strategy.entry_signal_volume_multiplier * data.avg_vol;
    }
    
    result.doji_pass = !detect_doji_pattern(data.curr.o, data.curr.h, data.curr.l, data.curr.c, config.strategy.doji_candlestick_body_size_threshold_percentage);
    result.all_pass = result.atr_pass && result.vol_pass && result.doji_pass;
    result.atr_ratio = (data.avg_atr > 0.0) ? (data.atr / data.avg_atr) : 0.0;
    result.vol_ratio = (data.avg_vol > 0.0) ? (data.curr.v / data.avg_vol) : 0.0;
    return result;
}

/**
 * @brief Calculate position size with multiple risk constraints
 * 
 * This function implements a comprehensive position sizing algorithm that considers:
 * 1. Risk per trade (% of equity to risk)
 * 2. Maximum exposure limits (% of equity in positions) 
 * 3. Maximum value per trade (dollar amount limit per trade)
 * 4. Available buying power (for margin/short selling)
 * 5. Existing positions (to prevent over-exposure)
 * 
 * The algorithm takes the MINIMUM of all constraints to ensure safe position sizing.
 * 
 * @param data Market and position data
 * @param equity Current account equity
 * @param current_qty Current position quantity (0 if no position)
 * @param config Trading configuration with risk parameters
 * @param buying_power Available buying power (0.0 if not provided)
 * @return PositionSizing struct with calculated quantity and risk amount
 */
PositionSizing calculate_position_sizing(const ProcessedData& data, double equity, int current_qty, const SystemConfig& config, double buying_power) {
    PositionSizing sizing;

    // Early return if price is invalid (zero or negative)
    if (data.curr.c <= 0.0) {
        sizing.quantity = 0;
        sizing.risk_based_qty = 0;
        sizing.exposure_based_qty = 0;
        sizing.max_value_qty = 0;
        sizing.buying_power_qty = 0;
        sizing.risk_amount = 0.0;
        return sizing;
    }

    double stop_loss_distance = data.atr;
    double risk_per_share = stop_loss_distance;

    // Calculate total risk budget (percentage of equity)
    double total_risk_budget = equity * config.strategy.risk_percentage_per_trade;

    // Set risk_amount to risk per share (ATR), not total budget
    sizing.risk_amount = risk_per_share;
    
    // Check for fixed shares per trade first (if enabled)
    if (config.strategy.enable_fixed_share_quantity_per_trade && config.strategy.fixed_share_quantity_per_trade > 0) {
        sizing.quantity = config.strategy.fixed_share_quantity_per_trade;
        sizing.risk_based_qty = 0;
        sizing.exposure_based_qty = 0;
        sizing.max_value_qty = 0;
        sizing.buying_power_qty = 0;
        
        // Apply position size multiplier to fixed shares if enabled
        if (config.strategy.enable_risk_based_position_multiplier) {
            sizing.quantity = static_cast<int>(sizing.quantity * config.strategy.risk_based_position_size_multiplier);
        }
        
        // Ensure minimum quantity of 1
        sizing.quantity = std::max(1, sizing.quantity);
        return sizing;
    }
    
    // Determine size multiplier for scaling in/out of positions
    sizing.size_multiplier = (current_qty != 0 && config.strategy.allow_multiple_positions_per_symbol)
                                 ? config.strategy.position_scaling_multiplier
                                 : 1.0;
    
    // Apply position size multiplier (if enabled)
    if (config.strategy.enable_risk_based_position_multiplier) {
        sizing.size_multiplier *= config.strategy.risk_based_position_size_multiplier;
    }
    
    // Calculate equity-based quantity with safety checks
    int equity_based_qty = 0;
    if (risk_per_share > 0.0 && total_risk_budget > 0.0 && sizing.size_multiplier > 0.0) {
        equity_based_qty = static_cast<int>(std::floor(
            (total_risk_budget * sizing.size_multiplier) / risk_per_share
        ));
    }

    double max_total_exposure_value = equity * (config.strategy.max_account_exposure_percentage / config.strategy.percentage_calculation_multiplier);
    double current_exposure_value = std::abs(data.pos_details.current_value);
    double available_exposure_value = max_total_exposure_value - current_exposure_value;

    available_exposure_value = std::max(0.0, available_exposure_value);

    // Calculate exposure-based quantity with safety checks
    int exposure_based_qty = 0;
    if (data.curr.c > 0.0 && available_exposure_value > 0.0) {
        exposure_based_qty = static_cast<int>(std::floor(available_exposure_value / data.curr.c));
    }
    
    sizing.quantity = std::min(equity_based_qty, exposure_based_qty);
    
    int max_value_qty = INT_MAX;
    if (config.strategy.maximum_dollar_value_per_trade > 0.0) {
        max_value_qty = static_cast<int>(std::floor(config.strategy.maximum_dollar_value_per_trade / data.curr.c));
        sizing.quantity = std::min(sizing.quantity, max_value_qty);
    }
    
    int buying_power_qty = INT_MAX;
    if (buying_power > 0.0) {
        double usable_buying_power = buying_power * config.strategy.buying_power_utilization_percentage;
        buying_power_qty = static_cast<int>(std::floor(usable_buying_power / data.curr.c));
        sizing.quantity = std::min(sizing.quantity, buying_power_qty);
    }
    
    sizing.risk_based_qty = equity_based_qty;
    sizing.exposure_based_qty = exposure_based_qty;
    sizing.max_value_qty = max_value_qty;
    sizing.buying_power_qty = buying_power_qty;
    
    if (sizing.quantity < 1 && equity_based_qty > 0) {
        sizing.quantity = 0;
    }
    
    return sizing;
}

/**
 * @brief Calculate exit targets (stop-loss and take-profit) for bracket orders
 * 
 * This function computes appropriate exit prices based on order direction and risk parameters.
 * It uses conservative buffers to handle data delays and broker validation requirements.
 * 
 * REALITY CHECK: "Real-time" data isn't truly free or comprehensive:
 * - Alpaca's free IEX data covers limited symbols and volume
 * - Most professional data feeds cost $100+/month
 * - Even "real-time" quotes can be stale by milliseconds
 * 
 * SOLUTION: Use larger buffers to account for market movement between
 * data fetch and order placement, preventing validation errors.
 * 
 * @param side Order side: "buy" for long positions, "sell" for short positions
 * @param entry_price Current market price to use as entry point
 * @param risk_amount Risk amount per share (typically ATR)
 * @param rr_ratio Risk-reward ratio (e.g., 3.0 means 3:1 reward:risk)
 * @return ExitTargets struct containing calculated stop_loss and take_profit prices
 */
ExitTargets compute_exit_targets(const std::string& side, double entry_price, double risk_amount, double rr_ratio, const SystemConfig& config) {
    ExitTargets targets;
    
    // Dynamic buffer system to handle data delays and market volatility
    // Values are now configurable per strategy
    double price_buffer_pct = config.strategy.price_buffer_pct;
    double min_price_buffer = config.strategy.min_price_buffer;
    double max_price_buffer = config.strategy.max_price_buffer;
    
    // Calculate dynamic buffer based on entry price
    double price_buffer = std::min(
        std::max(entry_price * price_buffer_pct, min_price_buffer),
        max_price_buffer
    );
    
    // Use the larger of risk_amount or calculated buffer for safety
    double effective_buffer = std::max(risk_amount, price_buffer);
    
    // Add minimum buffer to account for market data delays and price movements
    // Use configurable buffer for API validation and market delays
    double min_stop_buffer = config.strategy.stop_loss_buffer_amount_dollars;
    
    // Note: Order precision is limited by market data accuracy and API constraints.
    if (side == "buy") {
        // Long position: profit when price goes UP, stop when price goes DOWN
        // Ensure stop loss is well below entry price to account for market data delays
        targets.stop_loss = entry_price - std::max(effective_buffer, min_stop_buffer);
        
        // Calculate take profit using either percentage or risk/reward ratio
        if (config.strategy.use_take_profit_percentage) {
            targets.take_profit = entry_price * (1.0 + config.strategy.take_profit_percentage);
        } else {
            targets.take_profit = entry_price + (rr_ratio * risk_amount);
        }
    } else { // side == "sell" 
        // Short position: profit when price goes DOWN, stop when price goes UP
        // Ensure stop loss is well above entry price to account for market data delays
        targets.stop_loss = entry_price + std::max(effective_buffer, min_stop_buffer);
        
        // Calculate take profit using either percentage or risk/reward ratio
        if (config.strategy.use_take_profit_percentage) {
            targets.take_profit = entry_price * (1.0 - config.strategy.take_profit_percentage);
        } else {
            targets.take_profit = entry_price - (rr_ratio * risk_amount);
        }
    }
    
    return targets;
}

void process_signal_analysis(const ProcessedData& data, const SystemConfig& config) {
    SignalDecision signal_decision = detect_trading_signals(data, config);

    // Log candle data and enhanced signals table
    TradingLogs::log_candle_data_table(data.curr.o, data.curr.h, data.curr.l, data.curr.c);
    TradingLogs::log_signals_table_enhanced(signal_decision);

    // Enhanced detailed signal analysis logging
    TradingLogs::log_signal_analysis_detailed(data, signal_decision, config);

    FilterResult filter_result = evaluate_trading_filters(data, config);
    TradingLogs::log_filters(filter_result, config, data);
    TradingLogs::log_summary(data, signal_decision, filter_result, config.strategy.symbol);

    // CSV logging for signal analysis
    try {
        std::string timestamp = TimeUtils::get_current_human_readable_time();
        if (config.trading_mode.primary_symbol.empty()) {
            throw std::runtime_error("Primary symbol is required but not configured");
        }
        std::string symbol = config.trading_mode.primary_symbol;

        // Log signals to CSV
        if (AlpacaTrader::Logging::g_csv_trade_logger) {
            AlpacaTrader::Logging::g_csv_trade_logger->log_signal(
                timestamp, symbol, signal_decision.buy, signal_decision.sell,
                signal_decision.signal_strength, signal_decision.signal_reason
            );
        }

        // Log filters to CSV
        if (AlpacaTrader::Logging::g_csv_trade_logger) {
            AlpacaTrader::Logging::g_csv_trade_logger->log_filters(
                timestamp, symbol, filter_result.atr_pass, filter_result.atr_ratio,
                config.strategy.use_absolute_atr_threshold ?
                    config.strategy.atr_absolute_minimum_threshold :
                    config.strategy.entry_signal_atr_multiplier,
                filter_result.vol_pass, filter_result.vol_ratio,
                filter_result.doji_pass
            );
        }

        // Log market data to CSV
        if (AlpacaTrader::Logging::g_csv_trade_logger) {
            AlpacaTrader::Logging::g_csv_trade_logger->log_market_data(
                timestamp, symbol, data.curr.o, data.curr.h, data.curr.l, data.curr.c,
                data.curr.v, data.atr
            );
        }

    } catch (const std::exception& e) {
        TradingLogs::log_market_data_result_table("CSV logging error in signal analysis: " + std::string(e.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown CSV logging error in signal analysis", false, 0);
    }
}

std::pair<PositionSizing, SignalDecision> process_position_sizing(const ProcessedData& data, double equity, int current_qty, double buying_power, const SystemConfig& config) {
    PositionSizing sizing = calculate_position_sizing(data, equity, current_qty, config, buying_power);

    if (!evaluate_trading_filters(data, config).all_pass) {
        TradingLogs::log_filters_not_met_preview(sizing.risk_amount, sizing.quantity);

        // CSV logging for position sizing when filters not met
        try {
            std::string timestamp = TimeUtils::get_current_human_readable_time();
            if (config.trading_mode.primary_symbol.empty()) {
                throw std::runtime_error("Primary symbol is required but not configured");
            }
            std::string symbol = config.trading_mode.primary_symbol;

            if (AlpacaTrader::Logging::g_csv_trade_logger) {
                AlpacaTrader::Logging::g_csv_trade_logger->log_position_sizing(
                    timestamp, symbol, sizing.quantity, sizing.risk_amount,
                    sizing.quantity * data.curr.c, buying_power
                );
            }
        } catch (const std::exception& e) {
            TradingLogs::log_market_data_result_table("CSV logging error in position sizing: " + std::string(e.what()), false, 0);
        } catch (...) {
            TradingLogs::log_market_data_result_table("Unknown CSV logging error in position sizing", false, 0);
        }
        return {sizing, SignalDecision{}};
    }
    
    TradingLogs::log_filters_passed();
    TradingLogs::log_current_position(current_qty, config.strategy.symbol);
    TradingLogs::log_position_size_with_buying_power(sizing.risk_amount, sizing.quantity, buying_power, data.curr.c);
    TradingLogs::log_position_sizing_debug(sizing.risk_based_qty, sizing.exposure_based_qty, sizing.max_value_qty, sizing.buying_power_qty, sizing.quantity);

    SignalDecision signal_decision = detect_trading_signals(data, config);

    // CSV logging for successful position sizing
    try {
        std::string timestamp = TimeUtils::get_current_human_readable_time();
        if (config.trading_mode.primary_symbol.empty()) {
            throw std::runtime_error("Primary symbol is required but not configured");
        }
        std::string symbol = config.trading_mode.primary_symbol;

        if (AlpacaTrader::Logging::g_csv_trade_logger) {
            AlpacaTrader::Logging::g_csv_trade_logger->log_position_sizing(
                timestamp, symbol, sizing.quantity, sizing.risk_amount,
                sizing.quantity * data.curr.c, buying_power
            );
        }
    } catch (const std::exception& e) {
        TradingLogs::log_market_data_result_table("CSV logging error in successful position sizing: " + std::string(e.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown CSV logging error in successful position sizing", false, 0);
    }

    return {sizing, signal_decision};
}

} // namespace Core
} // namespace AlpacaTrader


