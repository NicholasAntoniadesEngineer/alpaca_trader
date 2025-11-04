#include "market_data_manager.hpp"
#include <stdexcept>
#include <cmath>

namespace AlpacaTrader {
namespace Core {

MarketDataManager::MarketDataManager(const SystemConfig& cfg, API::ApiManager& api_mgr, AccountManager& account_mgr)
    : config(cfg), api_manager(api_mgr), account_manager(account_mgr),
      market_data_fetcher(cfg),
      market_data_validator(cfg),
      market_bars_manager(cfg, api_mgr) {}

std::pair<ProcessedData, std::vector<Bar>> MarketDataManager::fetch_and_process_market_data() {
    std::vector<Bar> fetched_bars_data;
    
    try {
    // Fetch bars data once (will be used for snapshots and returned for CSV logging)
        fetched_bars_data = market_bars_manager.fetch_bars_data(config.strategy.symbol);
    
    // Fetch current snapshots using the bars we just fetched (avoids duplicate fetch)
    auto snapshots = fetch_current_snapshots_from_bars(fetched_bars_data);
    MarketSnapshot market_snapshot = snapshots.first;
    AccountSnapshot account_snapshot = snapshots.second;

        // Validate market snapshot - ATR can be 0.0 during accumulation, allow if price data is valid
        // If validation fails, check if we have minimal valid price data to proceed
        bool validation_passed = market_data_validator.validate_market_snapshot(market_snapshot);
        
        // DEBUG: Log validation result and snapshot state
        // Note: This debug info will be logged via coordinator's exception logging
        
        if (!validation_passed) {
            // Check if we have at least valid price data (even if ATR is 0 or other indicators are missing)
            bool has_minimal_price_data = market_snapshot.curr.close_price > 0.0 && 
                                           market_snapshot.curr.open_price > 0.0 &&
                                           market_snapshot.curr.high_price > 0.0 &&
                                           market_snapshot.curr.low_price > 0.0 &&
                                           market_snapshot.curr.high_price >= market_snapshot.curr.low_price &&
                                           market_snapshot.curr.high_price >= market_snapshot.curr.close_price &&
                                           market_snapshot.curr.low_price <= market_snapshot.curr.close_price;
            
            if (!has_minimal_price_data) {
                // No valid price data but bars were fetched - return empty ProcessedData with bars
                // Coordinator can use fallback logic to create snapshot from bars
                std::string validation_error_message = "Market snapshot validation failed - no valid price data. ";
                validation_error_message += "Bars fetched: " + std::to_string(fetched_bars_data.size()) + ". ";
                validation_error_message += "ATR: " + std::to_string(market_snapshot.atr) + ", ";
                validation_error_message += "Current price: " + std::to_string(market_snapshot.curr.close_price) + ". ";
                validation_error_message += "This may indicate: insufficient bars, invalid price data, or missing technical indicators.";
                // Return empty ProcessedData with bars so coordinator can use fallback
                return {ProcessedData{}, fetched_bars_data};
            }
            // Have minimal price data but validation failed (likely ATR=0 or missing indicators)
            // Continue anyway - trading will be blocked by data accumulation time check
    }

    // Create processed data from snapshots
    ProcessedData processed_data = ProcessedData(market_snapshot, account_snapshot);
    
    // DEBUG: Log bar data in ProcessedData after creation
    if (processed_data.curr.open_price > 0.0 && (processed_data.curr.high_price == 0.0 || processed_data.curr.low_price == 0.0 || processed_data.curr.close_price == 0.0)) {
        // Log warning - this will be caught by coordinator exception logging
        throw std::runtime_error("WARNING: ProcessedData created with incomplete bar data - O:" + 
            std::to_string(processed_data.curr.open_price) + " H:" + std::to_string(processed_data.curr.high_price) + 
            " L:" + std::to_string(processed_data.curr.low_price) + " C:" + std::to_string(processed_data.curr.close_price));
    }

        // Process account and position data
        if (!process_account_and_position_data(processed_data)) {
            throw std::runtime_error("Failed to process account and position data");
        }
        
        return {processed_data, fetched_bars_data};
    } catch (const std::exception& exception_error) {
        // Return empty ProcessedData with bars if exception occurs
        return {ProcessedData{}, fetched_bars_data};
    } catch (...) {
        // Return empty ProcessedData with bars if unknown exception occurs
        return {ProcessedData{}, fetched_bars_data};
    }
}

std::pair<MarketSnapshot, AccountSnapshot> MarketDataManager::fetch_current_snapshots() {
    // Fetch bars and create snapshots from them
    std::vector<Bar> bars_data = market_bars_manager.fetch_bars_data(config.strategy.symbol);
    return fetch_current_snapshots_from_bars(bars_data);
}

std::pair<MarketSnapshot, AccountSnapshot> MarketDataManager::fetch_current_snapshots_from_bars(const std::vector<Bar>& bars_data) {
    MarketSnapshot market_snapshot;
    AccountSnapshot account_snapshot;

    if (!bars_data.empty()) {
        market_snapshot = market_bars_manager.create_market_snapshot_from_bars(bars_data);
    }

    // Create account snapshot
    account_snapshot = create_account_snapshot();

    return {market_snapshot, account_snapshot};
}

QuoteData MarketDataManager::fetch_real_time_quote_data(const std::string& symbol) const {
    if (symbol.empty()) {
        throw std::runtime_error("Cannot fetch quote data: symbol is empty");
    }
    
    auto quote_data = api_manager.get_realtime_quotes(symbol);
    
    if (quote_data.mid_price <= 0.0) {
        throw std::runtime_error("Quote data fetch returned invalid price: " + std::to_string(quote_data.mid_price));
    }
    
    return quote_data;
}

bool MarketDataManager::wait_for_fresh_data(MarketDataSyncState& sync_state) {
    return market_data_fetcher.wait_for_fresh_data(sync_state);
}

bool MarketDataManager::set_sync_state_references(MarketDataSyncState& sync_state) {
    return market_data_fetcher.set_sync_state_references(sync_state);
}

bool MarketDataManager::is_data_fresh() const {
    return market_data_fetcher.is_data_fresh();
}

AccountSnapshot MarketDataManager::create_account_snapshot() const {
    AccountSnapshot account_snapshot;
    
    account_snapshot.equity = account_manager.fetch_account_equity();
    
    SymbolRequest symbol_request{config.strategy.symbol};
    account_snapshot.pos_details = account_manager.fetch_position_details(symbol_request);
    account_snapshot.open_orders = account_manager.fetch_open_orders_count(symbol_request);
    
    // Calculate exposure percentage
    if (account_snapshot.equity > 0.0) {
        account_snapshot.exposure_pct = (std::abs(account_snapshot.pos_details.current_value) / account_snapshot.equity) * config.strategy.percentage_calculation_multiplier;
    } else {
        account_snapshot.exposure_pct = 0.0;
    }

    return account_snapshot;
}

bool MarketDataManager::process_account_and_position_data(ProcessedData& processed_data) const {
    try {
        SymbolRequest symbol_request{config.strategy.symbol};
        processed_data.pos_details = account_manager.fetch_position_details(symbol_request);
        processed_data.open_orders = account_manager.fetch_open_orders_count(symbol_request);

        double account_equity = account_manager.fetch_account_equity();
        if (account_equity <= 0.0) {
            processed_data.exposure_pct = 0.0;
        } else {
            processed_data.exposure_pct = (std::abs(processed_data.pos_details.current_value) / account_equity) * config.strategy.percentage_calculation_multiplier;
        }
        
        return true;
    } catch (const std::exception& exception_error) {
        return false;
    } catch (...) {
        return false;
    }
}

} // namespace Core
} // namespace AlpacaTrader
