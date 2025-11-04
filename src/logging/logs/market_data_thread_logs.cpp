#include "market_data_thread_logs.hpp"
#include "logging/logger/async_logger.hpp"
#include "logging/logger/csv_bars_logger.hpp"
#include "utils/time_utils.hpp"
#include "logging/logger/logging_macros.hpp"
#include <sstream>
#include <iomanip>

using namespace AlpacaTrader::Logging;
using namespace AlpacaTrader::Core;
using namespace AlpacaTrader::API;
using namespace AlpacaTrader::Config;

void MarketDataThreadLogs::log_thread_startup(const SystemConfig& config) {
    log_message("MarketDataThread starting for symbol: " + config.strategy.symbol, "trading_system.log");
}

void MarketDataThreadLogs::log_thread_exception(const std::string& error_message) {
    log_message("MarketDataThread exception: " + error_message, "trading_system.log");
}

void MarketDataThreadLogs::log_thread_loop_exception(const std::string& error_message) {
    log_message("MarketDataThread loop iteration exception: " + error_message, "trading_system.log");
}

void MarketDataThreadLogs::log_market_data_fetch_start(const std::string& symbol, int bars_requested) {
    log_message("Requesting " + std::to_string(bars_requested) + " bars for " + symbol, "trading_system.log");
}

void MarketDataThreadLogs::log_market_data_fetch_result(const std::string& symbol, size_t bars_received) {
    log_message("Received " + std::to_string(bars_received) + " bars for " + symbol, "trading_system.log");
}

void MarketDataThreadLogs::log_insufficient_bars(const std::string& symbol, size_t bars_received, int bars_required) {
    log_message("Insufficient bars (" + std::to_string(bars_received) + " < " + std::to_string(bars_required) + ") for " + symbol, "trading_system.log");
}

void MarketDataThreadLogs::log_atr_calculation_result(const std::string& symbol, double atr_value, double current_price) {
    log_message("ATR computed for " + symbol + ": " + std::to_string(atr_value) + " (Price: $" + std::to_string(current_price) + ")", "trading_system.log");
}

void MarketDataThreadLogs::log_market_snapshot_update(const std::string& symbol) {
    log_message("Updating market snapshot for " + symbol, "trading_system.log");
}

void MarketDataThreadLogs::log_quote_fetch_start(const std::string& symbol) {
    log_message("Fetching real-time quotes for " + symbol, "trading_system.log");
}

void MarketDataThreadLogs::log_quote_fetch_result(const std::string& symbol, double mid_price, const std::string& timestamp) {
    log_message("Quote data received for " + symbol + " - Mid: $" + std::to_string(mid_price) + ", Timestamp: " + timestamp, "trading_system.log");
}

void MarketDataThreadLogs::log_quote_freshness_check(const std::string& symbol, bool is_fresh, int age_seconds) {
    log_message("Quote freshness check for " + symbol + " - Fresh: " + std::string(is_fresh ? "YES" : "NO") + " (age: " + std::to_string(age_seconds) + "s)", "trading_system.log");
}

void MarketDataThreadLogs::log_stale_quote_warning(const std::string& symbol, int age_seconds) {
    log_message("⚠️  CRYPTO DATA WARNING for " + symbol + ": Quote data is stale (age: " + std::to_string(age_seconds) + "s = " + std::to_string(age_seconds/3600) + "h), using bar data", "trading_system.log");
    log_message("⚠️  NOTE: Alpaca crypto data appears to be delayed/historical only, not real-time", "trading_system.log");
}

void MarketDataThreadLogs::log_csv_logging_decision(const std::string& symbol, bool should_log, int time_since_last_log) {
    auto csv_logger = get_logging_context()->csv_bars_logger;
    log_message("CSV Logger available: " + std::string(csv_logger ? "YES" : "NO"), "trading_system.log");
    log_message("Should log " + symbol + ": " + std::string(should_log ? "YES" : "NO") + " (time since last: " + std::to_string(time_since_last_log) + "s)", "trading_system.log");
}

void MarketDataThreadLogs::log_csv_quote_logging(const std::string& symbol, double mid_price) {
    log_message("Logged FRESH real-time quote data to CSV for " + symbol + " (Price: $" + std::to_string(mid_price) + ")", "trading_system.log");
}

void MarketDataThreadLogs::log_csv_bar_logging(const std::string& symbol, size_t bars_count) {
    log_message("Logging " + std::to_string(bars_count) + " bars to CSV for " + symbol + " (quotes stale/unavailable)", "trading_system.log");
    log_message("Successfully logged " + std::to_string(bars_count) + " bars to CSV for " + symbol, "trading_system.log");
}

void MarketDataThreadLogs::log_csv_logging_error(const std::string& symbol, const std::string& error_message) {
    log_message("CSV logging error for " + symbol + ": " + error_message, "trading_system.log");
}

void MarketDataThreadLogs::log_zero_atr_warning(const std::string& symbol) {
    log_message("ATR is zero for " + symbol + ", not updating snapshot", "trading_system.log");
}

void MarketDataThreadLogs::log_duplicate_bar_skipped(const std::string& symbol, const std::string& bar_timestamp) {
    log_message("Skipping bar logging for " + symbol + " - same historical data (latest bar: " + bar_timestamp + ")", "trading_system.log");
}

void MarketDataThreadLogs::log_fresh_quote_data_to_csv(const QuoteData& quote_data, const ProcessedData& processed_data, const std::string& timestamp) {
    auto csv_logger = get_logging_context()->csv_bars_logger;
    if (csv_logger) {
        csv_logger->log_market_data(
            timestamp, processed_data.curr.timestamp, quote_data.bid_price, quote_data.ask_price, 
            quote_data.bid_price, quote_data.mid_price, quote_data.ask_size + quote_data.bid_size,
            processed_data.atr, processed_data.avg_atr, processed_data.avg_vol
        );
    }
    log_message("Logged FRESH real-time quote data to CSV (Price: $" + std::to_string(quote_data.mid_price) + ")", "trading_system.log");
}

void MarketDataThreadLogs::log_historical_bars_to_csv(const std::vector<Bar>& historical_bars, const ProcessedData& processed_data, const std::string& timestamp, const std::string& symbol) {
    auto csv_logger2 = get_logging_context()->csv_bars_logger;
    if (csv_logger2) {
        // Log ALL bars that were fetched, not just the last one
        // Convert bar timestamps from milliseconds to human-readable format
        for (const auto& bar : historical_bars) {
            std::string bar_timestamp;
            if (bar.timestamp.empty()) {
                bar_timestamp = timestamp;
            } else {
                // Convert milliseconds timestamp to human-readable format
                bar_timestamp = TimeUtils::convert_milliseconds_to_human_readable(bar.timestamp);
            }
            
            csv_logger2->log_bar(
                bar_timestamp, symbol, bar, processed_data.atr, processed_data.avg_atr, processed_data.avg_vol
            );
        }
    }
    log_message("Successfully logged " + std::to_string(historical_bars.size()) + " bars to CSV", "trading_system.log");
}

bool MarketDataThreadLogs::is_fetch_allowed(const std::atomic<bool>* allow_fetch_ptr) {
    return allow_fetch_ptr && allow_fetch_ptr->load();
}

void MarketDataThreadLogs::process_csv_logging_if_needed(const ProcessedData& computed_data, const std::vector<Bar>& historical_bars, MarketDataValidator& validator, const std::string& symbol, const TimingConfig& timing, ApiManager& api_manager, std::chrono::steady_clock::time_point& last_bar_log_time, Bar& previous_bar) {
    (void)validator;  // Not used - we log bars directly without validating quotes
    (void)api_manager;  // Not used - we don't make API calls for CSV logging
    
    auto csv_logger = get_logging_context()->csv_bars_logger;
    if (!csv_logger) {
        return;
    }
    
    auto current_time = std::chrono::steady_clock::now();
    auto time_since_last_log = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_bar_log_time).count();
    bool should_log_csv_data = last_bar_log_time == std::chrono::steady_clock::time_point{} || 
                              time_since_last_log >= timing.market_data_logging_interval_seconds;
    
    MarketDataThreadLogs::log_csv_logging_decision(symbol, should_log_csv_data, time_since_last_log);
    
    if (!should_log_csv_data) {
        LOG_THREAD_CONTENT("Skipping CSV logging - too soon since last log (" + std::to_string(time_since_last_log) + "s, need " + std::to_string(timing.market_data_logging_interval_seconds) + "s)");
        return;
    }
    
    try {
        std::string current_timestamp = TimeUtils::get_current_human_readable_time();
        
        // Log bars directly without making API calls for quotes
        // Bars are already logged in market_data_coordinator, but this provides additional logging
        // with duplicate detection to avoid logging the same bar multiple times
        if (!historical_bars.empty()) {
            const auto& latest_bar = historical_bars.back();
            if (previous_bar.timestamp.empty() || latest_bar.timestamp != previous_bar.timestamp) {
                MarketDataThreadLogs::log_historical_bars_to_csv(historical_bars, computed_data, current_timestamp, symbol);
                // Update previous_bar to track the latest bar we logged
                previous_bar = latest_bar;
            } else {
                MarketDataThreadLogs::log_duplicate_bar_skipped(symbol, latest_bar.timestamp);
            }
        } else {
            log_message("No historical bars available for CSV logging", "trading_system.log");
        }
        
        last_bar_log_time = current_time;
    } catch (const std::exception& exception) {
        MarketDataThreadLogs::log_csv_logging_error(symbol, exception.what());
    } catch (...) {
        MarketDataThreadLogs::log_csv_logging_error(symbol, "Unknown error");
    }
}

