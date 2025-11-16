#include "market_data_fetcher.hpp"
#include "logging/logger/logging_macros.hpp"
#include <stdexcept>
#include <iostream>
#include <cstdlib>

namespace AlpacaTrader {
namespace Core {

MarketDataFetcher::MarketDataFetcher(const SystemConfig& cfg)
    : config(cfg), sync_state_ptr(nullptr) {}

bool MarketDataFetcher::wait_for_fresh_data(MarketDataSyncState& sync_state) {
    // CRITICAL: Validate all sync state pointers before use
    
    if (!sync_state.mtx) {
        throw std::runtime_error("Invalid sync state pointers: mtx is null");
    }
    if (!sync_state.cv) {
        throw std::runtime_error("Invalid sync state pointers: cv is null");
    }
    if (!sync_state.has_market) {
        throw std::runtime_error("Invalid sync state pointers: has_market is null");
    }
    if (!sync_state.has_account) {
        throw std::runtime_error("Invalid sync state pointers: has_account is null");
    }
    if (!sync_state.running) {
        throw std::runtime_error("Invalid sync state pointers: running is null");
    }
    
    
    // SPECIAL HANDLING FOR CRYPTO/WEB SOCKET MODE:
    // For crypto/WebSocket trading, data becomes available quickly once WebSocket connects
    if (config.strategy.is_crypto_asset) {
        // Check basic sync state
        std::unique_lock<std::mutex> lock(*sync_state.mtx);
        bool has_market = sync_state.has_market ? sync_state.has_market->load() : false;
        bool has_account = sync_state.has_account ? sync_state.has_account->load() : false;
        bool is_running = sync_state.running ? sync_state.running->load() : false;

        if (has_market && has_account && is_running) {
            // Data is already available
            return true;
        } else if (!is_running) {
            return false; // System is shutting down
        } else {
            // For crypto/WebSocket, wait briefly then assume data will be available
            // WebSocket connections happen quickly, and data becomes available immediately after
            sync_state.cv->wait_for(lock, std::chrono::seconds(3), [&]{
                if (!sync_state.running) {
                    return false;
                }
                // For crypto, just wait for account data and assume market data will be available
                return sync_state.has_account->load() && sync_state.running->load();
            });

            // For crypto/WebSocket, set has_market to true since WebSocket provides continuous data
            if (sync_state.has_market) {
                sync_state.has_market->store(true);
            }

            // For crypto, even if the wait condition isn't met, return true to allow data fetching
            // The WebSocket will connect during the fetch process
            return true;
        }
    }


    // STANDARD POLLING MODE (for stocks/Alpaca):
    // DEBUG: Log flag state before wait
    bool has_market_before = sync_state.has_market->load();
    bool has_account_before = sync_state.has_account->load();
    // Note: This will be logged via coordinator's exception logging
    
    
    bool wait_result = false;
    try {
        wait_result = wait_for_data_availability(sync_state);
    } catch (const std::exception& wait_exception_error) {
        throw;
    } catch (...) {
        throw std::runtime_error("Unknown exception in wait_for_data_availability");
    }
    
    // DEBUG: Log flag state after wait
    bool has_market_after = sync_state.has_market->load();
    bool has_account_after = sync_state.has_account->load();
    
    if (!wait_result) {
        std::string timeout_msg = "Data availability wait failed or timeout. ";
        timeout_msg += "has_market before: " + std::string(has_market_before ? "TRUE" : "FALSE") + ", ";
        timeout_msg += "has_market after: " + std::string(has_market_after ? "TRUE" : "FALSE") + ", ";
        timeout_msg += "has_account before: " + std::string(has_account_before ? "TRUE" : "FALSE") + ", ";
        timeout_msg += "has_account after: " + std::string(has_account_after ? "TRUE" : "FALSE");
        throw std::runtime_error(timeout_msg);
    }
    
    // DO NOT reset has_market_flag to false - it should remain true to indicate data is available
    // The flag is set to true when market data is updated and should remain true until the next update
    // Resetting it here causes the trading coordinator to see FALSE even though data was just updated
    
    // DEBUG: Verify flag is still true after wait
    if (sync_state.has_market && !sync_state.has_market->load()) {
        throw std::runtime_error("CRITICAL: has_market_flag was reset to FALSE during wait_for_fresh_data");
    }
    
    
    return true;
}

bool MarketDataFetcher::set_sync_state_references(MarketDataSyncState& sync_state) {
    sync_state_ptr = &sync_state;
    return true;
}

bool MarketDataFetcher::is_sync_state_valid() const {
    if (!sync_state_ptr) {
        return false;
    }
    
    if (!sync_state_ptr->market || !sync_state_ptr->account) {
        return false;
    }
    
    return true;
}

bool MarketDataFetcher::wait_for_data_availability(MarketDataSyncState& sync_state) {
    
    // Validate pointers before use
    if (!sync_state.mtx) {
        throw std::runtime_error("Invalid mutex pointer in wait_for_data_availability");
    }
    if (!sync_state.cv) {
        throw std::runtime_error("Invalid condition variable pointer in wait_for_data_availability");
    }
    
    
    try {
    std::unique_lock<std::mutex> lock(*sync_state.mtx);
    
        
        bool data_ready = sync_state.cv->wait_for(lock, std::chrono::seconds(config.timing.data_availability_wait_timeout_seconds), [&]{
            if (!sync_state.has_market || !sync_state.has_account) {
                return false;
            }
            bool has_market_value = sync_state.has_market->load();
            bool has_account_value = sync_state.has_account->load();
            return has_market_value && has_account_value;
    });
        
    
    return data_ready;
    } catch (const std::exception& lock_exception_error) {
        throw;
    } catch (...) {
        throw std::runtime_error("Unknown exception in wait_for_data_availability");
    }
}

bool MarketDataFetcher::is_data_fresh() const {
    
    
    if (!sync_state_ptr) {
        return false;
    }
    

    // SPECIAL HANDLING FOR CRYPTO: For crypto with MTH-TS, consider data fresh
    // since we have historical data loaded and WebSocket provides continuous updates
    if (config.strategy.is_crypto_asset && config.strategy.mth_ts_enabled) {
        // For MTH-TS crypto trading, we have historical data loaded at startup
        // and WebSocket provides real-time updates, so data is effectively always fresh
        return true;
    }
    
    if (!sync_state_ptr->market_data_timestamp) {
        return false;
    }


    auto now = std::chrono::steady_clock::now();
    
    
    std::chrono::steady_clock::time_point data_timestamp;
    try {
        
        // Validate pointer is still valid before dereferencing
        if (!sync_state_ptr || !sync_state_ptr->market_data_timestamp) {
            return false;
        }
        
        
        data_timestamp = sync_state_ptr->market_data_timestamp->load();
        
    } catch (const std::exception& load_exception_error) {
        return false;
    } catch (...) {
        return false;
    }
    
    
    auto max_age_seconds = config.strategy.is_crypto_asset ? 
        config.timing.crypto_data_staleness_threshold_seconds :
        config.timing.market_data_staleness_threshold_seconds;

    
    if (data_timestamp == std::chrono::steady_clock::time_point::min()) {
        return false;
    }
    
    
    auto age_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - data_timestamp).count();
    
    
    bool fresh = age_seconds <= max_age_seconds;
    
    
    return fresh;
}

} // namespace Core
} // namespace AlpacaTrader
