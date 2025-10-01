// TimingConfig.hpp
#ifndef TIMING_CONFIG_HPP
#define TIMING_CONFIG_HPP

struct ThreadPriorityConfig {
    bool enable_thread_priorities;
    bool enable_cpu_affinity;
    int trader_cpu_affinity;       // Pin trader to CPU
    int market_data_cpu_affinity;  // Pin market data to CPU
    bool log_thread_info;       // Log thread priority info on startup
};

struct TimingConfig {
    // Default constructor to initialize all values to 0
    TimingConfig() : thread_market_data_poll_interval_sec(0), thread_account_data_poll_interval_sec(0), 
                     thread_market_gate_poll_interval_sec(0), thread_trader_poll_interval_sec(0), 
                     thread_logging_poll_interval_sec(0), bar_fetch_minutes(0), bar_buffer(0),
                     pre_open_buffer_min(0), post_close_buffer_min(0), market_close_buffer_min(0),
                     halt_sleep_min(0), countdown_tick_sec(0), enable_thread_monitoring(false),
                     monitoring_interval_sec(0), account_data_cache_duration_sec(0),
                     market_data_max_age_seconds(0), thread_initialization_delay_ms(0), 
                     thread_startup_delay_ms(0), order_cancellation_wait_ms(0), 
                     position_verification_wait_ms(0), position_settlement_wait_ms(0), 
                     max_concurrent_cancellations(0), min_order_interval_sec(0), 
                     enable_wash_trade_prevention(false), cpu_usage_precision(0), rate_precision(0) {}
    
    // Thread polling intervals
    int thread_market_data_poll_interval_sec;
    int thread_account_data_poll_interval_sec;
    int thread_market_gate_poll_interval_sec;
    int thread_trader_poll_interval_sec;
    int thread_logging_poll_interval_sec;
    
    // Data configuration
    int bar_fetch_minutes;
    int bar_buffer;
    int pre_open_buffer_min;
    int post_close_buffer_min;
    int market_close_buffer_min;
    int halt_sleep_min;
    int countdown_tick_sec;
    bool enable_thread_monitoring;
    int monitoring_interval_sec;
    int account_data_cache_duration_sec;
    int market_data_max_age_seconds;
    
    // Thread initialization delays
    int thread_initialization_delay_ms;     // Delay before setting thread priorities
    int thread_startup_delay_ms;            // Delay for thread startup synchronization
    
    // Order cancellation timing
    int order_cancellation_wait_ms;         // Wait time after cancelling orders
    int position_verification_wait_ms;      // Wait time for position verification
    int position_settlement_wait_ms;        // Wait time for position settlement
    
    // Order cancellation concurrency
    int max_concurrent_cancellations;       // Maximum concurrent order cancellations
    
    // Order timing constraints
    int min_order_interval_sec;             // Minimum time between orders to prevent wash trades
    bool enable_wash_trade_prevention;      // Enable/disable wash trade prevention
    
    // Thread priority configuration
    ThreadPriorityConfig thread_priorities;
    
    // Precision configuration for performance metrics
    int cpu_usage_precision;             // Decimal places for CPU usage percentages
    int rate_precision;                  // Decimal places for rates (e.g., iterations/sec)
};

#endif // TIMING_CONFIG_HPP


