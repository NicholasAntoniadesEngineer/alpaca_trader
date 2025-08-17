// TimingConfig.hpp
#ifndef TIMING_CONFIG_HPP
#define TIMING_CONFIG_HPP

struct TimingConfig {
    int sleep_interval_sec;
    int account_poll_sec;
    int bar_fetch_minutes;
    int bar_buffer;
    int market_open_check_sec;
    int pre_open_buffer_min;
    int post_close_buffer_min;
    int halt_sleep_min;
    int countdown_tick_sec;
};

#endif // TIMING_CONFIG_HPP


