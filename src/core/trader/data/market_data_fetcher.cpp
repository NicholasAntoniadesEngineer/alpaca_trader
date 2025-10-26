#include "market_data_fetcher.hpp"
#include "core/logging/market_data_logs.hpp"
#include "core/logging/csv_bars_logger.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/system/system_state.hpp"
#include "core/utils/time_utils.hpp"
#include "core/trader/analysis/indicators.hpp"
#include <cmath>
#include <thread>
#include <chrono>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::MarketDataLogs;
using AlpacaTrader::Logging::TradingLogs;

MarketDataFetcher::MarketDataFetcher(API::ApiManager& api_mgr, AccountManager& account_mgr, const SystemConfig& cfg)
    : api_manager(api_mgr), account_manager(account_mgr), config(cfg) {}

ProcessedData MarketDataFetcher::fetch_and_process_data() {
    ProcessedData data;
    MarketDataLogs::log_market_data_fetch_table(config.strategy.symbol, config.logging.log_file);

    // Fetch market bars and validate data sufficiency
    if (!fetch_and_validate_market_bars(data)) {
        return data;
    }

    // Compute technical indicators
    if (!AlpacaTrader::Core::compute_technical_indicators(data, cached_bars, config)) {
        return data;
    }

    // Fetch account and position data
    account_manager.fetch_account_and_position_data(data);

    // Log current positions and check for warnings
    MarketDataLogs::log_position_data_and_warnings(
        data.pos_details.qty, 
        data.pos_details.current_value, 
        data.pos_details.unrealized_pl, 
        data.exposure_pct, 
        data.open_orders, 
        config.logging.log_file
    );

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
    auto bars = api_manager.get_recent_bars(br);

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

bool MarketDataFetcher::is_market_open() const {
    if (!api_manager.is_within_trading_hours(config.trading_mode.primary_symbol)) {
        TradingLogs::log_market_status(false, "Market is closed - outside trading hours");
        return false;
    }
    TradingLogs::log_market_status(true, "Market is open - trading allowed");
    return true;
}

bool MarketDataFetcher::is_data_fresh() const {
    // Check if sync_state_ptr is properly initialized
    if (!sync_state_ptr || !sync_state_ptr->market_data_timestamp) {
        TradingLogs::log_market_status(false, "Data sync not initialized - market_data_timestamp is null");
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto data_timestamp = sync_state_ptr->market_data_timestamp->load();
    
    // Use crypto-specific staleness threshold for 24/7 markets
    auto max_age_seconds = config.strategy.is_crypto_asset ? 
        config.timing.crypto_data_staleness_threshold_seconds : // Crypto: use crypto-specific threshold
        config.timing.market_data_staleness_threshold_seconds; // Stocks: use standard threshold

    // Additional null check before using the timestamp (defensive programming)
    if (!sync_state_ptr->market_data_timestamp) {
        TradingLogs::log_market_status(false, "Data sync lost during execution - market_data_timestamp became null");
        return false;
    }
    
    if (data_timestamp == std::chrono::steady_clock::time_point::min()) {
        TradingLogs::log_market_status(false, "Market data timestamp is invalid - no data received yet");
        return false;
    }
    
    auto age_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - data_timestamp).count();
    bool fresh = age_seconds <= max_age_seconds;
    
    if (!fresh) {
        TradingLogs::log_market_status(false, "Market data is stale - age: " + std::to_string(age_seconds) + 
                                    "s, max: " + std::to_string(max_age_seconds) + "s");
    } else {
        TradingLogs::log_market_status(true, "Market data is fresh - age: " + std::to_string(age_seconds) + "s");
    }
    
    return fresh;
}

bool MarketDataFetcher::validate_market_data(const MarketSnapshot& market) const {
    // Check if this is a default/empty MarketSnapshot (no data available)
    if (market.atr == 0.0 && market.avg_atr == 0.0 && market.avg_vol == 0.0 && 
        market.curr.o == 0.0 && market.curr.h == 0.0 && market.curr.l == 0.0 && market.curr.c == 0.0) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "No Data Available",
            "Symbol may not exist or market is closed",
            0,
            config.logging.log_file
        );
        return false;
    }

    // Validate that market data is not null/empty
    if (std::isnan(market.curr.c) || std::isnan(market.atr) || !std::isfinite(market.curr.c) || !std::isfinite(market.atr)) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Invalid Data",
            "NaN or infinite values detected in market data",
            0,
            config.logging.log_file
        );
        return false;
    }

    if (market.curr.c <= 0.0) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Invalid Data",
            "Price is zero or negative",
            0,
            config.logging.log_file
        );
        return false;
    }

    if (market.atr <= 0.0) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Insufficient Data",
            "ATR is zero or negative - insufficient volatility data for trading",
            0,
            config.logging.log_file
        );
        return false;
    }

    // Validate OHLC data is reasonable (H >= L, H >= C, L <= C)
    if (market.curr.h < market.curr.l || market.curr.h < market.curr.c || market.curr.l > market.curr.c) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Invalid Data",
            "OHLC relationship violation - invalid price data structure",
            0,
            config.logging.log_file
        );
        return false;
    }

    return true;
}

} // namespace Core
} // namespace AlpacaTrader
