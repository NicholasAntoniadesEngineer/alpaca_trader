#ifndef STRATEGY_CONFIG_HPP
#define STRATEGY_CONFIG_HPP

#include <string>

struct StrategyConfig {
    // ========================================================================
    // TARGET AND SESSION CONFIGURATION
    // ========================================================================

    std::string symbol;                              // Target symbol for trading
    bool is_crypto_asset;                            // Whether the target symbol is a cryptocurrency

    // Market session timing (Eastern Time)
    int et_utc_offset_hours;                         // UTC offset for Eastern Time
    int market_open_hour;                            // Market open hour (Eastern Time)
    int market_open_minute;                          // Market open minute (Eastern Time)
    int market_close_hour;                           // Market close hour (Eastern Time)
    int market_close_minute;                         // Market close minute (Eastern Time)

    // ========================================================================
    // CORE STRATEGY SETTINGS
    // ========================================================================

    // Signal detection and confirmation requirements
    bool buy_signals_allow_equal_close;              // Allow close >= open for buy signals
    bool buy_signals_require_higher_high;            // Require current high > previous high
    bool buy_signals_require_higher_low;             // Require current low >= previous low
    bool sell_signals_allow_equal_close;             // Allow close <= open for sell signals
    bool sell_signals_require_lower_low;             // Require current low < previous low
    bool sell_signals_require_lower_high;            // Require current high <= previous high

    // Signal filter thresholds (Conservative Settings)
    double entry_signal_atr_multiplier;              // ATR multiplier for entry signals
    double entry_signal_volume_multiplier;           // Volume multiplier for entry signals
    double crypto_volume_multiplier;                 // Crypto-specific volume multiplier (fractional volumes)
    double crypto_volume_change_amplification_factor; // Amplification factor for crypto volume change percentages
    double percentage_calculation_multiplier;        // Multiplier for percentage calculations (default 100.0)
    double minimum_volume_threshold;                 // Minimum volume threshold to avoid division by zero
    int average_atr_comparison_multiplier;           // Average ATR comparison multiplier

    // Volatility calculation configuration
    int bars_to_fetch_for_calculations;              // Number of bars to fetch for ATR and other calculations
    int minutes_per_bar;                             // Minutes per bar (timeframe for market data)
    int atr_calculation_bars;                        // ATR calculation period in bars (preferred, but ATR adapts to available bars)
    int minimum_bars_for_atr_calculation;            // Minimum number of bars required for ATR calculation (ATR returns 0.0 if fewer bars available)
    std::string daily_bars_timeframe;                // Timeframe for daily bars (e.g., "1Day")
    int daily_bars_count;                            // Number of daily bars to fetch for historical comparison
    int minimum_data_accumulation_seconds_before_trading;  // Minimum seconds of data accumulation required before allowing trades

    // ATR-based signal validation
    double atr_absolute_minimum_threshold;           // Absolute ATR minimum threshold
    bool use_absolute_atr_threshold;                 // Use absolute ATR threshold instead of relative

    // Momentum signal requirements (for signal strength)
    double minimum_price_change_percentage_for_momentum;  // Min price change % for momentum signals
    double minimum_volume_increase_percentage_for_buy_signals;  // Min volume increase % for buy signals
    double minimum_volatility_percentage_for_buy_signals;      // Min volatility % for buy signals
    double minimum_volume_increase_percentage_for_sell_signals; // Min volume increase % for sell signals
    double minimum_volatility_percentage_for_sell_signals;     // Min volatility % for sell signals
    double minimum_signal_strength_threshold;        // Minimum signal strength threshold (0.0 to 1.0)

    // Signal strength component weighting
    double basic_price_pattern_weight;               // Weight for basic pattern in signal strength
    double momentum_indicator_weight;                // Weight for momentum in signal strength
    double volume_analysis_weight;                   // Weight for volume in signal strength
    double volatility_analysis_weight;               // Weight for volatility in signal strength

    // Candlestick pattern detection
    double doji_candlestick_body_size_threshold_percentage;  // Doji body size threshold as % of total range

    // ========================================================================
    // RISK MANAGEMENT CONFIGURATION
    // ========================================================================

    // Daily Trading Limits (as percentages of account value)
    double max_daily_loss_percentage;                // Maximum daily loss as percentage of account value
    double daily_profit_target_percentage;           // Daily profit target as percentage of account value
    double max_account_exposure_percentage;          // Maximum account exposure as percentage
    double position_scaling_multiplier;              // Multiplier for scaling in/out of positions

    // Per-Trade Risk Management
    double risk_percentage_per_trade;                // Risk percentage per trade
    double maximum_dollar_value_per_trade;           // Maximum dollar value per individual trade
    bool allow_multiple_positions_per_symbol;        // Allow multiple positions for the same symbol
    int maximum_position_layers;                     // Maximum number of layers for a position
    bool close_positions_on_signal_reversal;         // Close positions on signal reversal

    // Buying Power Management
    double buying_power_utilization_percentage;      // Percentage of available buying power to use
    double buying_power_validation_safety_margin;    // Safety margin for buying power validation

    // ========================================================================
    // POSITION MANAGEMENT CONFIGURATION
    // ========================================================================

    // Position sizing strategy selection
    bool enable_fixed_share_quantity_per_trade;      // Enable fixed shares per trade
    bool enable_risk_based_position_multiplier;     // Enable position size multiplier
    int fixed_share_quantity_per_trade;              // Fixed number of shares per trade
    double risk_based_position_size_multiplier;      // Multiplier for position size
    int maximum_share_quantity_per_single_trade;     // Maximum quantity allowed per trade
    double maximum_dollar_value_per_single_trade;    // Maximum order value in dollars

    // ========================================================================
    // PROFIT TAKING AND EXIT STRATEGY
    // ========================================================================

    // Take profit configuration (choose ONE method)
    double take_profit_percentage;                   // Take profit as percentage of entry price
    bool use_take_profit_percentage;                 // Use percentage-based take profit
    double rr_ratio;                                 // Risk/reward ratio for exit calculations
    double profit_taking_threshold_dollars;          // Automatic sell when profit exceeds this amount
    bool enable_profit_taking;                       // Enable automatic profit taking

    // Price buffer for order execution
    double price_buffer_pct;                         // Percentage of entry price for buffer
    double min_price_buffer;                         // Minimum buffer amount in dollars
    double max_price_buffer;                         // Maximum buffer amount in dollars

    // Stop loss order configuration
    double stop_loss_buffer_amount_dollars;          // Additional buffer in dollars for stop loss
    bool use_current_market_price_for_order_execution;  // Use real-time price for order calculations

    // ========================================================================
    // SHORT SELLING CONFIGURATION
    // ========================================================================

    bool enable_short_selling;                       // Enable short selling functionality
    bool short_availability_check;                   // Check short availability before placing orders
    int default_shortable_quantity;                  // Default shortable quantity when not available
    double existing_short_multiplier;                // Multiplier for existing short positions
    double short_safety_margin;                      // Safety margin for short selling (0.0-1.0)
    int short_retry_attempts;                        // Number of retry attempts for short orders
    int short_retry_delay_ms;                        // Delay between retry attempts in milliseconds

    // ========================================================================
    // ORDER MANAGEMENT CONFIGURATION
    // ========================================================================

    // Order validation
    int min_quantity;                                // Minimum order quantity
    int price_precision;                             // Decimal places for price formatting
    std::string default_time_in_force;               // Default time in force for orders
    std::string default_order_type;                  // Default order type

    // Position closure settings
    std::string position_closure_side_buy;           // Side for buying to close short positions
    std::string position_closure_side_sell;          // Side for selling to close long positions

    // API endpoints
    std::string orders_endpoint;                     // Orders API endpoint
    std::string positions_endpoint;                  // Positions API endpoint
    std::string orders_status_filter;                // Default status filter for orders

    // Error handling
    bool zero_quantity_check;                        // Enable zero quantity validation
    bool position_verification_enabled;              // Enable position verification

    // Order cancellation strategy
    std::string cancellation_mode;                   // Cancellation strategy mode
    bool cancel_opposite_side;                       // Cancel opposite side orders
    bool cancel_same_side;                           // Cancel same side orders
    int max_orders_to_cancel;                        // Maximum orders to cancel at once

    // ========================================================================
    // SIGNAL VALIDATION CONSTRAINTS
    // ========================================================================

    double minimum_acceptable_price_for_signals;     // Minimum price threshold for validation
    double maximum_acceptable_price_for_signals;     // Maximum price threshold for validation

    // ========================================================================
    // ERROR HANDLING CONFIGURATION
    // ========================================================================

    // Retry configuration
    int max_retries;                                 // Maximum number of retries for failed operations
    int retry_delay_ms;                              // Delay between retries in milliseconds

    // Circuit breaker configuration
    bool circuit_breaker_enabled;                    // Enable circuit breaker for error protection
    int circuit_breaker_threshold;                   // Circuit breaker threshold (number of failures)
    int circuit_breaker_timeout_min;                 // Circuit breaker timeout in minutes

    // ========================================================================
    // MONITORING CONFIGURATION
    // ========================================================================

    // Health check configuration
    int health_check_interval_sec;                   // Health check interval in seconds
    int performance_report_interval_min;             // Performance report interval in minutes

    // Alert configuration
    bool alert_on_failure_rate;                      // Enable failure rate alerts
    double max_failure_rate_pct;                     // Maximum allowed failure rate percentage
    bool alert_on_drawdown;                          // Enable drawdown alerts
    double max_drawdown_pct;                         // Maximum allowed drawdown percentage
    bool alert_on_data_stale;                        // Enable data staleness alerts
    int max_data_age_min;                            // Maximum data age in minutes
    int max_inactivity_min;                          // Maximum inactivity period in minutes

    // ========================================================================
    // PRECISION SETTINGS
    // ========================================================================

    // Strategy precision configuration
    int ratio_display_precision;                     // Decimal places for ratios (e.g., rr_ratio)
    int factor_display_precision;                    // Decimal places for factors (e.g., multipliers)
    int atr_volume_display_precision;                // Decimal places for ATR and volume values

    // ========================================================================
    // SIGNAL AND POSITION LABEL CONFIGURATION
    // ========================================================================

    std::string signal_buy_string;                   // String label for buy signals
    std::string signal_sell_string;                  // String label for sell signals
    std::string position_long_string;                 // String label for long positions
    std::string position_short_string;                // String label for short positions
};

#endif // STRATEGY_CONFIG_HPP


