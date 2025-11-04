#ifndef CONNECTIVITY_MANAGER_HPP
#define CONNECTIVITY_MANAGER_HPP

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include "configs/timing_config.hpp"

/**
 * ConnectivityManager - Manages connectivity state
 * 
 * This class manages the connectivity status across the system,
 * providing a central point for all threads to check network connectivity and
 * coordinate retry behavior. It tracks both current connectivity status and
 * implements intelligent retry logic with exponential backoff.
 */
class ConnectivityManager {
public:
    enum class ConnectionStatus {
        CONNECTED,          // Network is working normally
        DEGRADED,          // Some failures but still attempting
        DISCONNECTED       // Multiple failures, backing off
    };

    struct ConnectivityState {
        ConnectionStatus status = ConnectionStatus::CONNECTED;
        std::chrono::steady_clock::time_point last_success;
        std::chrono::steady_clock::time_point last_failure;
        std::chrono::steady_clock::time_point next_retry_time;
        int consecutive_failures = 0;
        int retry_delay_seconds = 1;
        std::string last_error_message;
    };

private:
    mutable std::mutex state_mutex_;
    ConnectivityState state_;
    int max_retry_delay_seconds;
    int degraded_threshold;
    int disconnected_threshold;
    double backoff_multiplier;

public:
    explicit ConnectivityManager(const TimingConfig& timing_config);
    ~ConnectivityManager() = default;

    // Delete copy constructor and assignment operator
    ConnectivityManager(const ConnectivityManager&) = delete;
    ConnectivityManager& operator=(const ConnectivityManager&) = delete;

    void report_success();
    void report_failure(const std::string& error_message);
    bool should_attempt_connection() const;
    int get_seconds_until_retry() const;
    bool is_connectivity_outage() const;
    inline ConnectionStatus get_status() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_.status;
    }
    inline ConnectivityState get_state() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_;
    }
    std::string get_status_string() const;
    void reset_connectivity_state();
};

#endif // CONNECTIVITY_MANAGER_HPP