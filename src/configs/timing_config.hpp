// TimingConfig.hpp
#ifndef TIMING_CONFIG_HPP
#define TIMING_CONFIG_HPP

struct TimingConfig {
    // ========================================================================
    // THREAD POLLING INTERVALS
    // ========================================================================

    int thread_market_data_poll_interval_sec;        // Market data thread polling interval in seconds
    int thread_account_data_poll_interval_sec;       // Account data thread polling interval in seconds
    int thread_market_gate_poll_interval_sec;        // Market gate thread polling interval in seconds
    int thread_trader_poll_interval_sec;             // Trader decision thread polling interval in seconds
    int thread_logging_poll_interval_sec;            // Logging thread polling interval in seconds

    // ========================================================================
    // HISTORICAL DATA CONFIGURATION
    // ========================================================================

    int historical_data_fetch_period_minutes;        // Historical data fetch period in minutes
    int historical_data_buffer_size;                 // Historical data buffer size
    int account_data_cache_duration_seconds;         // Account data cache duration in seconds
    int market_data_staleness_threshold_seconds;     // Market data staleness threshold in seconds
    int crypto_data_staleness_threshold_seconds;     // Crypto-specific data staleness threshold in seconds
    int market_data_logging_interval_seconds;         // Market data CSV logging interval in seconds
    int quote_data_freshness_threshold_seconds;       // Quote data freshness threshold in seconds

    // ========================================================================
    // MARKET SESSION BUFFER TIMES
    // ========================================================================

    int pre_market_open_buffer_minutes;              // Pre-market open buffer time in minutes
    int post_market_close_buffer_minutes;            // Post-market close buffer time in minutes
    int market_close_grace_period_minutes;           // Market close grace period in minutes

    // ========================================================================
    // SYSTEM HEALTH MONITORING
    // ========================================================================

    bool enable_system_health_monitoring;            // Enable system health monitoring
    int system_health_logging_interval_seconds;      // System health logging interval in seconds

    // ========================================================================
    // ERROR RECOVERY TIMING
    // ========================================================================

    int emergency_trading_halt_duration_minutes;     // Emergency trading halt duration in minutes

    // ========================================================================
    // USER INTERFACE UPDATES
    // ========================================================================

    int countdown_display_refresh_interval_seconds;  // Countdown display refresh interval in seconds

    // ========================================================================
    // THREAD LIFECYCLE MANAGEMENT
    // ========================================================================

    int thread_initialization_delay_milliseconds;    // Thread initialization delay in milliseconds
    int thread_startup_sequence_delay_milliseconds;  // Thread startup sequence delay in milliseconds

    // ========================================================================
    // ORDER MANAGEMENT TIMING
    // ========================================================================

    int order_cancellation_processing_delay_milliseconds;  // Order cancellation processing delay in milliseconds
    int position_verification_timeout_milliseconds;        // Position verification timeout in milliseconds
    int position_settlement_timeout_milliseconds;          // Position settlement timeout in milliseconds
    int maximum_concurrent_order_cancellations;            // Maximum concurrent order cancellations

    // ========================================================================
    // TRADING SAFETY CONSTRAINTS
    // ========================================================================

    int minimum_interval_between_orders_seconds;     // Minimum time between orders to prevent wash trades
    bool enable_wash_trade_prevention_mechanism;     // Enable/disable wash trade prevention mechanism

    // ========================================================================
    // PRECISION SETTINGS FOR METRICS
    // ========================================================================

    int cpu_usage_display_precision;                 // Decimal places for CPU usage percentages
    int performance_rate_display_precision;          // Decimal places for rates (e.g., iterations/sec)
};

#endif // TIMING_CONFIG_HPP


