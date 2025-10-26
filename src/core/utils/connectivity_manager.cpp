#include "connectivity_manager.hpp"

ConnectivityManager::ConnectivityManager() {
    state_.last_success = std::chrono::steady_clock::now();
    state_.next_retry_time = std::chrono::steady_clock::now();
}

void ConnectivityManager::report_success() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    auto now = std::chrono::steady_clock::now();
    
    state_.status = ConnectionStatus::CONNECTED;
    state_.last_success = now;
    state_.consecutive_failures = 0;
    state_.retry_delay_seconds = 1;
    state_.next_retry_time = now;
    state_.last_error_message.clear();
}

void ConnectivityManager::report_failure(const std::string& error_message) {
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

bool ConnectivityManager::should_attempt_connection() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    auto now = std::chrono::steady_clock::now();
    
    // Always allow if connected
    if (state_.status == ConnectionStatus::CONNECTED) {
        return true;
    }
    
    // Check if enough time has passed for retry
    return now >= state_.next_retry_time;
}

ConnectivityManager::ConnectionStatus ConnectivityManager::get_status() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_.status;
}

ConnectivityManager::ConnectivityState ConnectivityManager::get_state() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_;
}

int ConnectivityManager::get_seconds_until_retry() const {
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

bool ConnectivityManager::is_connectivity_outage() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_.status == ConnectionStatus::DISCONNECTED;
}

std::string ConnectivityManager::get_status_string() const {
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

void ConnectivityManager::reset_connectivity_state() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    auto now = std::chrono::steady_clock::now();
    
    state_.status = ConnectionStatus::CONNECTED;
    state_.consecutive_failures = 0;
    state_.retry_delay_seconds = 1;
    state_.next_retry_time = now;
    state_.last_error_message.clear();
}

bool ConnectivityManager::check_connectivity() const {
    return !is_connectivity_outage();
}

bool ConnectivityManager::check_connectivity_status() const {
    if (is_connectivity_outage()) {
        std::string connectivity_msg = "Connectivity outage - status: " + get_status_string();
        // Note: This method doesn't have access to TradingLogs, so we'll let the caller handle logging
        return false;
    }
    return true;
}
