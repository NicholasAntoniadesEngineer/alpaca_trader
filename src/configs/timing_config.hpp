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
    int monitoring_interval_sec;
    int account_data_cache_duration_sec;
    
    // Thread initialization delays
    int thread_initialization_delay_ms;     // Delay before setting thread priorities
    int thread_startup_delay_ms;            // Delay for thread startup synchronization
    
    // Thread priority configuration
    ThreadPriorityConfig thread_priorities;
    
    // Precision configuration for performance metrics
    int cpu_usage_precision;             // Decimal places for CPU usage percentages
    int rate_precision;                  // Decimal places for rates (e.g., iterations/sec)
};

#endif // TIMING_CONFIG_HPP


