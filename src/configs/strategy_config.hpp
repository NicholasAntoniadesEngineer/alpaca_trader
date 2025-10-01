// StrategyConfig.hpp
#ifndef STRATEGY_CONFIG_HPP
#define STRATEGY_CONFIG_HPP

struct StrategyConfig {
    int atr_period;
    double atr_multiplier_entry;
    double volume_multiplier;

    double rr_ratio;
    int avg_atr_multiplier;
    double atr_absolute_threshold;              // Absolute ATR threshold for trading (e.g., 0.9)
    bool use_absolute_atr_threshold;            // Use absolute ATR threshold instead of relative
    
    // MOMENTUM SIGNAL CONFIGURATION (Production-Ready)
    double min_price_change_pct;                // Minimum price change percentage for signals
    double min_volume_change_pct;               // Minimum volume change percentage for buy signals
    double min_volatility_pct;                  // Minimum volatility percentage for buy signals
    double min_sell_volume_change_pct;          // Minimum volume change percentage for sell signals
    double min_sell_volatility_pct;             // Minimum volatility percentage for sell signals
    double signal_strength_threshold;           // Minimum signal strength threshold (0.0 to 1.0)
    
    // SIGNAL STRENGTH WEIGHTING (Configurable)
    double basic_pattern_weight;                // Weight for basic pattern in signal strength (0.0 to 1.0)
    double momentum_weight;                     // Weight for momentum in signal strength (0.0 to 1.0)
    double volume_weight;                       // Weight for volume in signal strength (0.0 to 1.0)
    double volatility_weight;                   // Weight for volatility in signal strength (0.0 to 1.0)
    
    // DOJI PATTERN DETECTION (Configurable)
    double doji_body_threshold;                 // Doji body size threshold as percentage of total range
    
    // Price buffer configuration for order execution
    double price_buffer_pct;                 // Percentage of entry price for buffer
    double min_price_buffer;                 // Minimum buffer amount in dollars
    double max_price_buffer;                 // Maximum buffer amount in dollars
    
    // Stop loss buffer configuration for API validation
    double stop_loss_buffer_dollars;         // Additional buffer in dollars for stop loss validation
    bool use_realtime_price_for_orders;      // Use real-time price for order calculations
    
    // Profit taking configuration
    double profit_taking_threshold_dollars;  // Automatic sell when profit exceeds this amount
    double take_profit_percentage;           // Take profit as percentage of entry price (e.g., 0.02 = 2%)
    bool use_take_profit_percentage;         // Use percentage-based take profit instead of risk/reward ratio
    bool enable_profit_taking;               // Enable automatic profit taking
    
    // Signal detection configuration
    bool buy_allow_equal_close;              // Allow close >= open for buy signals
    bool buy_require_higher_high;            // Require current high > previous high
    bool buy_require_higher_low;             // Require current low >= previous low
    
    bool sell_allow_equal_close;             // Allow close <= open for sell signals  
    bool sell_require_lower_low;             // Require current low < previous low
    bool sell_require_lower_high;            // Require current high <= previous high
    
    // Position scaling configuration
    bool enable_fixed_shares;                // Enable fixed shares per trade
    bool enable_position_multiplier;         // Enable position size multiplier
    int fixed_shares_per_trade;              // Fixed number of shares per trade (only used if enable_fixed_shares is true)
    double position_size_multiplier;         // Multiplier for position size (only used if enable_position_multiplier is true)
    
    // Order validation configuration
    int max_quantity_per_trade;              // Maximum quantity allowed per trade
    double min_price_threshold;              // Minimum price threshold for validation
    double max_price_threshold;              // Maximum price threshold for validation
    double max_order_value;                  // Maximum order value in dollars
    
    // Short selling configuration
    bool enable_short_selling;               // Enable short selling functionality
    bool short_availability_check;           // Check short availability before placing orders
    int default_shortable_quantity;          // Default shortable quantity when not available
    double existing_short_multiplier;        // Multiplier for existing short positions
    double short_safety_margin;              // Safety margin for short selling (0.0-1.0)
    int short_retry_attempts;                // Number of retry attempts for short orders
    int short_retry_delay_ms;                // Delay between retry attempts in milliseconds
    
    // Monitoring configuration
    int health_check_interval_sec;           // Health check interval in seconds
    int performance_report_interval_min;     // Performance report interval in minutes
    bool alert_on_failure_rate;              // Enable failure rate alerts
    double max_failure_rate_pct;             // Maximum allowed failure rate percentage
    bool alert_on_drawdown;                  // Enable drawdown alerts
    double max_drawdown_pct;                 // Maximum allowed drawdown percentage
    bool alert_on_data_stale;                // Enable data staleness alerts
    int max_data_age_min;                    // Maximum data age in minutes
    int max_inactivity_min;                  // Maximum inactivity in minutes
    
    // Error handling configuration
    int max_retries;                         // Maximum number of retries for failed operations
    int retry_delay_ms;                      // Delay between retries in milliseconds
    bool circuit_breaker_enabled;            // Enable circuit breaker for error protection
    int circuit_breaker_threshold;           // Circuit breaker threshold (number of failures)
    int circuit_breaker_timeout_min;         // Circuit breaker timeout in minutes
    
    // Precision configuration for logging and display
    int ratio_precision;                     // Decimal places for ratios (e.g., rr_ratio)
    int factor_precision;                    // Decimal places for factors (e.g., multipliers)
    int atr_vol_precision;                   // Decimal places for ATR and volume values
};

#endif // STRATEGY_CONFIG_HPP


