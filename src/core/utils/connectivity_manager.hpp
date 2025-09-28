#ifndef CONNECTIVITY_MANAGER_HPP
#define CONNECTIVITY_MANAGER_HPP

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

/**
 * ConnectivityManager - Shared connectivity state for all threads
 * 
 * This singleton class manages the connectivity status across the entire system,
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
    
    // Configuration constants
    static constexpr int MAX_RETRY_DELAY = 5;        // 5 minutes max backoff
    static constexpr int DEGRADED_THRESHOLD = 3;       // Failures before degraded
    static constexpr int DISCONNECTED_THRESHOLD = 6;   // Failures before disconnected
    static constexpr double BACKOFF_MULTIPLIER = 2.0;  // Exponential backoff factor

    ConnectivityManager();

public:
    // Singleton access
    static ConnectivityManager& instance();

    // Delete copy constructor and assignment operator
    ConnectivityManager(const ConnectivityManager&) = delete;
    ConnectivityManager& operator=(const ConnectivityManager&) = delete;

    void report_success();
    void report_failure(const std::string& error_message);
    bool should_attempt_connection() const;
    ConnectionStatus get_status() const;
    ConnectivityState get_state() const;
    int get_seconds_until_retry() const;
    bool is_connectivity_outage() const;
    std::string get_status_string() const;
    void reset_connectivity_state();
};

#endif // CONNECTIVITY_MANAGER_HPP