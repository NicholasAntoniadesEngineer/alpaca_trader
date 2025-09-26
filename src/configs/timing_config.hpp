// TimingConfig.hpp
#ifndef TIMING_CONFIG_HPP
#define TIMING_CONFIG_HPP

struct ThreadPriorityConfig {
    bool enable_thread_priorities = true;
    bool enable_cpu_affinity = false;
    int trader_cpu_affinity = 0;       // Pin trader to CPU 0
    int market_data_cpu_affinity = 1;  // Pin market data to CPU 1
    bool log_thread_info = true;       // Log thread priority info on startup
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
    // Thread priority configuration
    ThreadPriorityConfig thread_priorities;
};

#endif // TIMING_CONFIG_HPP


