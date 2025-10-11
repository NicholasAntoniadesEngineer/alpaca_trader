#include "market_data_fetcher.hpp"
#include "core/logging/market_data_logs.hpp"
#include "core/system/system_state.hpp"
#include <cmath>
#include <thread>
#include <chrono>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::MarketDataLogs;

MarketDataFetcher::MarketDataFetcher(API::AlpacaClient& client_ref, AccountManager& account_mgr, const SystemConfig& cfg)
    : client(client_ref), account_manager(account_mgr), config(cfg) {}

ProcessedData MarketDataFetcher::fetch_and_process_data() {
    ProcessedData data;
    MarketDataLogs::log_market_data_fetch_table(config.strategy.symbol, config.logging.log_file);

    // Fetch market bars and validate data sufficiency
    if (!fetch_and_validate_market_bars(data)) {
        return data;
    }

    // Compute technical indicators
    if (!compute_technical_indicators(data)) {
        return data;
    }

    // Fetch account and position data
    fetch_account_and_position_data(data);

    // Log current positions and check for warnings
    log_position_data_and_warnings(data);

    return data;
}

std::pair<MarketSnapshot, AccountSnapshot> MarketDataFetcher::fetch_current_snapshots() {
    if (!is_sync_state_valid()) {
        return {MarketSnapshot{}, AccountSnapshot{}};
    }
    
    return {*sync_state_ptr->market, *sync_state_ptr->account};
}

void MarketDataFetcher::wait_for_fresh_data(MarketDataSyncState& sync_state) {
    try {
        if (!validate_sync_state_pointers(sync_state)) {
            MarketDataLogs::log_sync_state_error("Invalid sync state pointers", config.logging.log_file);
            return;
        }
        
        if (!wait_for_data_availability(sync_state)) {
            return;
        }
        
        MarketDataLogs::log_data_available(config.logging.log_file);
        mark_data_as_consumed(sync_state);
        
    } catch (const std::exception& e) {
        MarketDataLogs::log_data_exception("Exception in wait_for_fresh_data: " + std::string(e.what()), config.logging.log_file);
    } catch (...) {
        MarketDataLogs::log_data_exception("Unknown exception in wait_for_fresh_data", config.logging.log_file);
    }
}

void MarketDataFetcher::set_sync_state_references(MarketDataSyncState& sync_state) {
    sync_state_ptr = &sync_state;
}

bool MarketDataFetcher::validate_sync_state_pointers(MarketDataSyncState& sync_state) const {
    return sync_state.mtx && sync_state.cv && 
           sync_state.has_market && sync_state.has_account && 
           sync_state.running;
}

void MarketDataFetcher::mark_data_as_consumed(MarketDataSyncState& sync_state) const {
    if (sync_state.has_market) {
        sync_state.has_market->store(false);
    }
}

// Private helper methods for improved readability

/**
 * Fetches market bars from the API and validates that we have sufficient data
 * for technical analysis. Returns false if data is insufficient.
 */
bool MarketDataFetcher::fetch_and_validate_market_bars(ProcessedData& data) {
    // Use configurable bars to fetch and ATR calculation bars
    const int bars_to_fetch = config.strategy.bars_to_fetch_for_calculations + config.timing.historical_data_buffer_size;
    BarRequest br{config.strategy.symbol, bars_to_fetch};
    auto bars = client.get_recent_bars(br);

    const size_t required_bars = static_cast<size_t>(config.strategy.atr_calculation_bars + 2);
    
    if (bars.size() < required_bars) {
        if (bars.empty()) {
            MarketDataLogs::log_market_data_result_table("No market data available", false, 0, config.logging.log_file);
        } else {
            MarketDataLogs::log_market_data_result_table("Insufficient data for analysis", false, bars.size(), config.logging.log_file);
        }
        MarketDataLogs::log_market_data_result_table("Market data collection failed", false, bars.size(), config.logging.log_file);
        return false;
    }
    
    // Store bars in member variable for later processing
    cached_bars = std::move(bars);
    
    // Populate the ProcessedData with the latest bar information
    if (!cached_bars.empty()) {
        data.curr = cached_bars.back();
        constexpr size_t MINIMUM_BARS_FOR_PREV = 2;
        if (cached_bars.size() >= MINIMUM_BARS_FOR_PREV) {
            data.prev = cached_bars[cached_bars.size() - 2];
        }
    }
    
    return true;
}

/**
 * Computes technical indicators using the cached market bars.
 * Returns false if indicator computation fails.
 */
bool MarketDataFetcher::compute_technical_indicators(ProcessedData& data) {
    MarketDataLogs::log_market_data_attempt_table("Computing indicators", config.logging.log_file);
    
    data = MarketProcessing::compute_processed_data(cached_bars, config);
    
    if (data.atr == 0.0) {
        MarketDataLogs::log_market_data_result_table("Indicator computation failed", false, 0, config.logging.log_file);
        return false;
    }
    
    return true;
}

/**
 * Fetches current account and position data from the account manager.
 * Calculates exposure percentage based on current position value and account equity.
 */
void MarketDataFetcher::fetch_account_and_position_data(ProcessedData& data) {
    MarketDataLogs::log_market_data_attempt_table("Getting position and account data", config.logging.log_file);
    
    SymbolRequest sr{config.strategy.symbol};
    data.pos_details = account_manager.fetch_position_details(sr);
    data.open_orders = account_manager.fetch_open_orders_count(sr);
    
    double equity = account_manager.fetch_account_equity();
    data.exposure_pct = calculate_exposure_percentage(data.pos_details.current_value, equity);
}

/**
 * Logs current position data and checks for trading warnings.
 * Issues a warning if there's a position without corresponding bracket orders.
 */
void MarketDataFetcher::log_position_data_and_warnings(const ProcessedData& data) {
    MarketDataLogs::log_current_positions_table(
        data.pos_details.qty, 
        data.pos_details.current_value, 
        data.pos_details.unrealized_pl, 
        data.exposure_pct, 
        data.open_orders, 
        config.logging.log_file
    );

    if (data.pos_details.qty != 0 && data.open_orders == 0) {
        MarketDataLogs::log_market_data_result_table("Missing bracket order warning", true, 0, config.logging.log_file);
    }
}

/**
 * Calculates the exposure percentage of the current position relative to account equity.
 * Returns 0.0 if equity is zero or negative.
 */
double MarketDataFetcher::calculate_exposure_percentage(double current_value, double equity) const {
    if (equity <= 0.0) {
        return 0.0;
    }
    return (std::abs(current_value) / equity) * 100.0;
}

// Synchronization helper methods

/**
 * Validates that the sync state pointer and its components are valid.
 * Returns false and logs error if validation fails.
 */
bool MarketDataFetcher::is_sync_state_valid() const {
    if (!sync_state_ptr) {
        MarketDataLogs::log_sync_state_error("No sync state available", config.logging.log_file);
        return false;
    }
    
    if (!sync_state_ptr->market || !sync_state_ptr->account) {
        MarketDataLogs::log_sync_state_error("Invalid snapshot pointers", config.logging.log_file);
        return false;
    }
    
    return true;
}

/**
 * Waits for both market and account data to be available.
 * Returns false if timeout occurs or data is not available.
 */
bool MarketDataFetcher::wait_for_data_availability(MarketDataSyncState& sync_state) {
    std::unique_lock<std::mutex> lock(*sync_state.mtx);
    
    bool data_ready = sync_state.cv->wait_for(lock, std::chrono::seconds(1), [&]{
        return (sync_state.has_market && sync_state.has_market->load()) && 
               (sync_state.has_account && sync_state.has_account->load());
    });
    
    if (!data_ready) {
        MarketDataLogs::log_data_timeout(config.logging.log_file);
        return false;
    }
    
    return true;
}

} // namespace Core
} // namespace AlpacaTrader
