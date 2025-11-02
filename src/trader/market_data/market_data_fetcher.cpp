#include "market_data_fetcher.hpp"
#include <stdexcept>

namespace AlpacaTrader {
namespace Core {

MarketDataFetcher::MarketDataFetcher(const SystemConfig& cfg)
    : config(cfg), sync_state_ptr(nullptr) {}

bool MarketDataFetcher::wait_for_fresh_data(MarketDataSyncState& sync_state) {
    // Validate sync state pointers
    if (!sync_state.mtx || !sync_state.cv || 
        !sync_state.has_market || !sync_state.has_account || 
        !sync_state.running) {
        throw std::runtime_error("Invalid sync state pointers");
    }
    
    if (!wait_for_data_availability(sync_state)) {
        throw std::runtime_error("Data availability wait failed or timeout");
    }
    
    // Mark data as consumed
    if (sync_state.has_market) {
        sync_state.has_market->store(false);
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
    std::unique_lock<std::mutex> lock(*sync_state.mtx);
    
    bool data_ready = sync_state.cv->wait_for(lock, std::chrono::seconds(1), [&]{
        return (sync_state.has_market && sync_state.has_market->load()) && 
               (sync_state.has_account && sync_state.has_account->load());
    });
    
    return data_ready;
}

bool MarketDataFetcher::is_data_fresh() const {
    if (!sync_state_ptr || !sync_state_ptr->market_data_timestamp) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto data_timestamp = sync_state_ptr->market_data_timestamp->load();
    
    auto max_age_seconds = config.strategy.is_crypto_asset ? 
        config.timing.crypto_data_staleness_threshold_seconds :
        config.timing.market_data_staleness_threshold_seconds;

    if (!sync_state_ptr->market_data_timestamp) {
        return false;
    }
    
    if (data_timestamp == std::chrono::steady_clock::time_point::min()) {
        return false;
    }
    
    auto age_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - data_timestamp).count();
    bool fresh = age_seconds <= max_age_seconds;
    
    return fresh;
}

} // namespace Core
} // namespace AlpacaTrader
