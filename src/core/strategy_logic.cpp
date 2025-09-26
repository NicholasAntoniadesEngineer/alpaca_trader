// strategy_logic.cpp
#include "core/strategy_logic.hpp"
#include "indicators.hpp"
#include <cmath>
#include <climits>

namespace AlpacaTrader {
namespace Core {

namespace StrategyLogic {

SignalDecision detect_signals(const ProcessedData& data, const TraderConfig& config) {
    SignalDecision decision;
    
    // Configurable BUY signal conditions
    bool buy_close_condition = config.strategy.buy_allow_equal_close ? 
                              (data.curr.c >= data.curr.o) : 
                              (data.curr.c > data.curr.o);
    
    bool buy_high_condition = config.strategy.buy_require_higher_high ? 
                             (data.curr.h > data.prev.h) : 
                             true;
    
    bool buy_low_condition = config.strategy.buy_require_higher_low ? 
                            (data.curr.l >= data.prev.l) : 
                            true;
    
    decision.buy = buy_close_condition && buy_high_condition && buy_low_condition;
    
    // Configurable SELL signal conditions
    bool sell_close_condition = config.strategy.sell_allow_equal_close ? 
                               (data.curr.c <= data.curr.o) : 
                               (data.curr.c < data.curr.o);
    
    bool sell_low_condition = config.strategy.sell_require_lower_low ? 
                             (data.curr.l < data.prev.l) : 
                             true;
    
    bool sell_high_condition = config.strategy.sell_require_lower_high ? 
                              (data.curr.h <= data.prev.h) : 
                              true;
    
    decision.sell = sell_close_condition && sell_low_condition && sell_high_condition;
    
    return decision;
}

FilterResult evaluate_filters(const ProcessedData& data, const TraderConfig& config) {
    FilterResult result;
    result.atr_pass = data.atr > config.strategy.atr_multiplier_entry * data.avg_atr;
    result.vol_pass = data.curr.v > config.strategy.volume_multiplier * data.avg_vol;
    result.doji_pass = !is_doji(data.curr.o, data.curr.h, data.curr.l, data.curr.c);
    result.all_pass = result.atr_pass && result.vol_pass && result.doji_pass;
    result.atr_ratio = (data.avg_atr > 0.0) ? (data.atr / data.avg_atr) : 0.0;
    result.vol_ratio = (data.avg_vol > 0.0) ? (static_cast<double>(data.curr.v) / data.avg_vol) : 0.0;
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
PositionSizing calculate_position_sizing(const ProcessedData& data, double equity, int current_qty, const TraderConfig& config, double buying_power) {
    PositionSizing sizing;
    sizing.risk_amount = data.atr;  // Use ATR as risk per share
    
    // Determine size multiplier for scaling in/out of positions
    sizing.size_multiplier = (current_qty != 0 && config.risk.allow_multiple_positions)
                                 ? config.risk.scale_in_multiplier
                                 : 1.0;
    
    // CONSTRAINT 1: Risk-based sizing (risk_per_trade % of equity)
    int equity_based_qty = static_cast<int>(std::floor(
        (equity * config.risk.risk_per_trade * sizing.size_multiplier) / sizing.risk_amount
    ));
    
    // CONSTRAINT 2: Exposure-based sizing (max_exposure_pct % of equity total)
    double max_total_exposure_value = equity * (config.risk.max_exposure_pct / 100.0);
    double current_exposure_value = std::abs(data.pos_details.current_value);
    double available_exposure_value = max_total_exposure_value - current_exposure_value;
    
    // Prevent negative available exposure
    available_exposure_value = std::max(0.0, available_exposure_value);
    
    int exposure_based_qty = static_cast<int>(std::floor(available_exposure_value / data.curr.c));
    
    // Take the minimum of risk-based and exposure-based constraints
    sizing.quantity = std::min(equity_based_qty, exposure_based_qty);
    
    // CONSTRAINT 3: Maximum value per trade limitation (if configured)
    int max_value_qty = INT_MAX; // Default to no max value constraint
    if (config.risk.max_value_per_trade > 0.0) {
        max_value_qty = static_cast<int>(std::floor(config.risk.max_value_per_trade / data.curr.c));
        
        // Apply the most restrictive constraint so far
        sizing.quantity = std::min(sizing.quantity, max_value_qty);
    }
    
    // CONSTRAINT 4: Buying power limitation (if provided)
    int buying_power_qty = INT_MAX; // Default to no buying power constraint
    if (buying_power > 0.0) {
        // Apply configurable safety factor (default 80% of buying power)
        double usable_buying_power = buying_power * config.risk.buying_power_usage_factor;
        buying_power_qty = static_cast<int>(std::floor(usable_buying_power / data.curr.c));
        
        // Apply the most restrictive constraint
        sizing.quantity = std::min(sizing.quantity, buying_power_qty);
    }
    
    // Store debug information for logging
    sizing.risk_based_qty = equity_based_qty;
    sizing.exposure_based_qty = exposure_based_qty;
    sizing.max_value_qty = max_value_qty;
    sizing.buying_power_qty = buying_power_qty;
    
    // Final validation: ensure we have a valid quantity
    if (sizing.quantity < 1 && equity_based_qty > 0) {
        sizing.quantity = 0; // Will trigger validation failure in trader
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
ExitTargets compute_exit_targets(const std::string& side, double entry_price, double risk_amount, double rr_ratio, const TraderConfig& config) {
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
    
    if (side == "buy") {
        // Long position: profit when price goes UP, stop when price goes DOWN
        targets.stop_loss = entry_price - effective_buffer;
        targets.take_profit = entry_price + (rr_ratio * risk_amount);
    } else { // side == "sell" 
        // Short position: profit when price goes DOWN, stop when price goes UP
        targets.stop_loss = entry_price + effective_buffer;
        targets.take_profit = entry_price - (rr_ratio * risk_amount);
    }
    
    return targets;
}

} // namespace StrategyLogic
} // namespace Core
} // namespace AlpacaTrader


