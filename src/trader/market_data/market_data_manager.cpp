#include "market_data_manager.hpp"
#include "logging/logs/market_data_logs.hpp"
#include "logging/logs/trading_logs.hpp"
#include <stdexcept>
#include <cmath>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::MarketDataLogs;
using AlpacaTrader::Logging::TradingLogs;

MarketDataManager::MarketDataManager(const SystemConfig& cfg, API::ApiManager& api_mgr, AccountManager& account_mgr)
    : config(cfg), api_manager(api_mgr), account_manager(account_mgr),
      market_data_fetcher(cfg),
      market_data_validator(cfg),
      market_bars_manager(cfg, api_mgr) {}

ProcessedData MarketDataManager::fetch_and_process_market_data() {
    try {
        MarketDataLogs::log_market_data_fetch_table(config.strategy.symbol, config.logging.log_file);
        
        // Fetch current snapshots
        auto snapshots = fetch_current_snapshots();
        MarketSnapshot market_snapshot = snapshots.first;
        AccountSnapshot account_snapshot = snapshots.second;

        // Validate market snapshot
        if (!market_data_validator.validate_market_snapshot(market_snapshot)) {
            MarketDataLogs::log_market_data_failure_summary(
                config.strategy.symbol,
                "Validation Failed",
                "Market snapshot validation failed",
                0,
                config.logging.log_file
            );
            return ProcessedData{};
        }

        // Create processed data from snapshots
        ProcessedData processed_data = ProcessedData(market_snapshot, account_snapshot);

        // Process account and position data
        if (!process_account_and_position_data(processed_data)) {
            MarketDataLogs::log_market_data_failure_summary(
                config.strategy.symbol,
                "Processing Failed",
                "Failed to process account and position data",
                0,
                config.logging.log_file
            );
            return ProcessedData{};
        }

        // Log position data and warnings
        MarketDataLogs::log_position_data_and_warnings(
            processed_data.pos_details.position_quantity,
            processed_data.pos_details.current_value,
            processed_data.pos_details.unrealized_pl,
            processed_data.exposure_pct,
            processed_data.open_orders,
            config.logging.log_file,
            config.strategy.position_long_string,
            config.strategy.position_short_string
        );
        
        return processed_data;
        
    } catch (const std::runtime_error& runtime_error) {
        MarketDataLogs::log_market_data_failure_summary(
            config.strategy.symbol,
            "Runtime Error",
            runtime_error.what(),
            0,
            config.logging.log_file
        );
        return ProcessedData{};
    } catch (const std::exception& exception_error) {
        MarketDataLogs::log_market_data_failure_summary(
            config.strategy.symbol,
            "Exception",
            std::string("Exception in fetch_and_process_market_data: ") + exception_error.what(),
            0,
            config.logging.log_file
        );
        return ProcessedData{};
    } catch (...) {
        MarketDataLogs::log_market_data_failure_summary(
            config.strategy.symbol,
            "Unknown Error",
            "Unknown exception in fetch_and_process_market_data",
            0,
            config.logging.log_file
        );
        return ProcessedData{};
    }
}

std::pair<MarketSnapshot, AccountSnapshot> MarketDataManager::fetch_current_snapshots() {
    MarketSnapshot market_snapshot;
    AccountSnapshot account_snapshot;

    try {
        // Fetch bars data for market snapshot using MarketBarsManager
        std::vector<Bar> bars_data = market_bars_manager.fetch_bars_data(config.strategy.symbol);

        if (!bars_data.empty()) {
            market_snapshot = market_bars_manager.create_market_snapshot_from_bars(bars_data);
        }

        // Create account snapshot
        account_snapshot = create_account_snapshot();
    } catch (const std::exception& exception_error) {
        MarketDataLogs::log_market_data_failure_summary(
            config.strategy.symbol,
            "Exception",
            std::string("Exception in fetch_current_snapshots: ") + exception_error.what(),
            0,
            config.logging.log_file
        );
    }

    return {market_snapshot, account_snapshot};
}

QuoteData MarketDataManager::fetch_real_time_quote_data(const std::string& symbol) const {
    try {
        MarketDataLogs::log_market_data_fetch_table(symbol, config.logging.log_file);
        
        if (symbol.empty()) {
            throw std::runtime_error("Cannot fetch quote data: symbol is empty");
        }
        
        auto quote_data = api_manager.get_realtime_quotes(symbol);
        
        if (quote_data.mid_price <= 0.0) {
            throw std::runtime_error("Quote data fetch returned invalid price: " + std::to_string(quote_data.mid_price));
        }
        
        MarketDataLogs::log_market_data_result_table("Quote data fetched", true, static_cast<size_t>(quote_data.mid_price), config.logging.log_file);
        
        return quote_data;
        
    } catch (const std::runtime_error& runtime_error) {
        MarketDataLogs::log_market_data_failure_summary(
            symbol,
            "Runtime Error",
            runtime_error.what(),
            0,
            config.logging.log_file
        );
        return QuoteData{};
    } catch (const std::exception& exception_error) {
        MarketDataLogs::log_market_data_failure_summary(
            symbol,
            "Exception",
            std::string("Exception in fetch_real_time_quote_data: ") + exception_error.what(),
            0,
            config.logging.log_file
        );
        return QuoteData{};
    } catch (...) {
        MarketDataLogs::log_market_data_failure_summary(
            symbol,
            "Unknown Error",
            "Unknown exception in fetch_real_time_quote_data",
            0,
            config.logging.log_file
        );
        return QuoteData{};
    }
}

bool MarketDataManager::wait_for_fresh_data(MarketDataSyncState& sync_state) {
    try {
        bool wait_result = market_data_fetcher.wait_for_fresh_data(sync_state);
        if (wait_result) {
            MarketDataLogs::log_data_available(config.logging.log_file);
            return true;
        }
        return false;
        
    } catch (const std::runtime_error& runtime_error) {
        if (std::string(runtime_error.what()).find("Invalid sync state pointers") != std::string::npos) {
            MarketDataLogs::log_sync_state_error("Invalid sync state pointers", config.logging.log_file);
        } else if (std::string(runtime_error.what()).find("Data availability wait failed") != std::string::npos) {
            MarketDataLogs::log_data_timeout(config.logging.log_file);
        } else {
            MarketDataLogs::log_sync_state_error(std::string("Runtime error in wait_for_fresh_data: ") + runtime_error.what(), config.logging.log_file);
        }
        return false;
    } catch (const std::exception& exception_error) {
        MarketDataLogs::log_data_exception("Exception in wait_for_fresh_data: " + std::string(exception_error.what()), config.logging.log_file);
        return false;
    } catch (...) {
        MarketDataLogs::log_data_exception("Unknown exception in wait_for_fresh_data", config.logging.log_file);
        return false;
    }
}

bool MarketDataManager::set_sync_state_references(MarketDataSyncState& sync_state) {
    return market_data_fetcher.set_sync_state_references(sync_state);
}

bool MarketDataManager::is_data_fresh() const {
    bool fresh_result = market_data_fetcher.is_data_fresh();
    
    if (fresh_result) {
        TradingLogs::log_market_status(true, "Market data is fresh");
    } else {
        TradingLogs::log_market_status(false, "Market data is stale or unavailable");
    }
    
    return fresh_result;
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
