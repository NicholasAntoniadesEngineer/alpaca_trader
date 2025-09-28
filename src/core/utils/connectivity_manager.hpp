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

    ConnectivityManager() {
        state_.last_success = std::chrono::steady_clock::now();
        state_.next_retry_time = std::chrono::steady_clock::now();
    }

public:
    // Singleton access
    static ConnectivityManager& instance() {
        static ConnectivityManager instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    ConnectivityManager(const ConnectivityManager&) = delete;
    ConnectivityManager& operator=(const ConnectivityManager&) = delete;

    /**
     * Report a successful network operation
     */
    void report_success() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        auto now = std::chrono::steady_clock::now();
        
        state_.status = ConnectionStatus::CONNECTED;
        state_.last_success = now;
        state_.consecutive_failures = 0;
        state_.retry_delay_seconds = 1;
        state_.next_retry_time = now;
        state_.last_error_message.clear();
    }

    /**
     * Report a network failure with error message
     */
    void report_failure(const std::string& error_message) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        auto now = std::chrono::steady_clock::now();
        
        state_.last_failure = now;
        state_.consecutive_failures++;
        state_.last_error_message = error_message;
        
        // Update status based on failure count
        if (state_.consecutive_failures >= DISCONNECTED_THRESHOLD) {
            state_.status = ConnectionStatus::DISCONNECTED;
        } else if (state_.consecutive_failures >= DEGRADED_THRESHOLD) {
            state_.status = ConnectionStatus::DEGRADED;
        }
        
        // Calculate exponential backoff
        state_.retry_delay_seconds = std::min(
            static_cast<int>(state_.retry_delay_seconds * BACKOFF_MULTIPLIER),
            MAX_RETRY_DELAY
        );
        
        state_.next_retry_time = now + std::chrono::seconds(state_.retry_delay_seconds);
    }

    /**
     * Check if network operations should be attempted
     */
    bool should_attempt_connection() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        auto now = std::chrono::steady_clock::now();
        
        // Always allow if connected
        if (state_.status == ConnectionStatus::CONNECTED) {
            return true;
        }
        
        // Check if enough time has passed for retry
        return now >= state_.next_retry_time;
    }

    /**
     * Get current connectivity status
     */
    ConnectionStatus get_status() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_.status;
    }

    /**
     * Get full connectivity state (for logging/monitoring)
     */
    ConnectivityState get_state() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_;
    }

    /**
     * Get seconds until next retry attempt
     */
    int get_seconds_until_retry() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        auto now = std::chrono::steady_clock::now();
        
        if (state_.next_retry_time <= now) {
            return 0;
        }
        
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            state_.next_retry_time - now
        );
        return static_cast<int>(duration.count());
    }

    /**
     * Check if we're in a connectivity outage (for halting trading)
     */
    bool is_connectivity_outage() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_.status == ConnectionStatus::DISCONNECTED;
    }

    /**
     * Get human-readable status string
     */
    std::string get_status_string() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        switch (state_.status) {
            case ConnectionStatus::CONNECTED:
                return "CONNECTED";
            case ConnectionStatus::DEGRADED:
                return "DEGRADED";
            case ConnectionStatus::DISCONNECTED:
                return "DISCONNECTED";
            default:
                return "UNKNOWN";
        }
    }

    /**
     * Force a connectivity check reset (for testing/manual intervention)
     */
    void reset_connectivity_state() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        auto now = std::chrono::steady_clock::now();
        
        state_.status = ConnectionStatus::CONNECTED;
        state_.consecutive_failures = 0;
        state_.retry_delay_seconds = 1;
        state_.next_retry_time = now;
        state_.last_error_message.clear();
    }
};

#endif // CONNECTIVITY_MANAGER_HPP
