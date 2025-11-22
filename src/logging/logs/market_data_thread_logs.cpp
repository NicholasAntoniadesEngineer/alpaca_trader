#include "market_data_thread_logs.hpp"
#include "logging/logger/async_logger.hpp"
#include "logging/logger/csv_bars_logger.hpp"
#include "utils/time_utils.hpp"
#include "logging/logger/logging_macros.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

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
    TABLE_HEADER_48("MARKET DATA REQUEST", "Historical Bars Fetch");

    TABLE_ROW_48("Symbol", symbol);
    TABLE_ROW_48("Bars Requested", std::to_string(bars_requested));
    TABLE_ROW_48("Data Type", "HISTORICAL BARS");
    TABLE_ROW_48("Status", "REQUEST INITIATED");

    TABLE_FOOTER_48();
}

void MarketDataThreadLogs::log_market_data_fetch_result(const std::string& symbol, size_t bars_received) {
    TABLE_HEADER_48("MARKET DATA RECEIVED", "Historical Bars Response");

    TABLE_ROW_48("Symbol", symbol);
    TABLE_ROW_48("Bars Received", std::to_string(bars_received));
    TABLE_ROW_48("Data Type", "HISTORICAL BARS");
    TABLE_ROW_48("Status", bars_received > 0 ? "SUCCESS" : "NO DATA");

    TABLE_FOOTER_48();
}

void MarketDataThreadLogs::log_insufficient_bars(const std::string& symbol, size_t bars_received, int bars_required) {
    TABLE_HEADER_48("INSUFFICIENT DATA", "Market Data Accumulation Required");

    TABLE_ROW_48("Symbol", symbol);
    TABLE_ROW_48("Bars Received", std::to_string(bars_received));
    TABLE_ROW_48("Bars Required", std::to_string(bars_required));
    TABLE_ROW_48("Deficit", std::to_string(bars_required - (int)bars_received) + " bars");

    TABLE_SEPARATOR_48();

    TABLE_ROW_48("STATUS", "WAITING FOR MORE DATA");
    TABLE_ROW_48("ACTION", "Will retry on next cycle");

    TABLE_FOOTER_48();
}

void MarketDataThreadLogs::log_atr_calculation_result(const std::string& symbol, double atr_value, double current_price) {
    TABLE_HEADER_48("ATR CALCULATION", "Volatility Assessment");

    TABLE_ROW_48("Symbol", symbol);
    TABLE_ROW_48("ATR Value", std::to_string(atr_value).substr(0,6));
    TABLE_ROW_48("Current Price", "$" + std::to_string(current_price).substr(0,8));
    TABLE_ROW_48("ATR %", std::to_string((atr_value / current_price) * 100.0).substr(0,4) + "%");

    TABLE_SEPARATOR_48();

    std::string volatility_assessment;
    double atr_percent = (atr_value / current_price) * 100.0;
    if (atr_percent < 0.1) volatility_assessment = "VERY LOW VOLATILITY";
    else if (atr_percent < 0.5) volatility_assessment = "LOW VOLATILITY";
    else if (atr_percent < 1.0) volatility_assessment = "MODERATE VOLATILITY";
    else if (atr_percent < 2.0) volatility_assessment = "HIGH VOLATILITY";
    else volatility_assessment = "EXTREME VOLATILITY";

    TABLE_ROW_48("VOLATILITY LEVEL", volatility_assessment);
    TABLE_ROW_48("TRADING STATUS", atr_value > 0.0 ? "READY" : "INVALID DATA");

    TABLE_FOOTER_48();
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

void MarketDataThreadLogs::log_csv_logging_decision(const std::string& symbol, bool should_log, int time_since_last_log, const LoggingConfig&) {
    auto csv_logger = get_logging_context()->csv_bars_logger;

    TABLE_HEADER_48("CSV LOGGING DECISION", "Data Recording Status");

    TABLE_ROW_48("Symbol", symbol);
    TABLE_ROW_48("CSV Logger", csv_logger ? "AVAILABLE" : "UNAVAILABLE");
    TABLE_ROW_48("Time Since Last Log", std::to_string(time_since_last_log) + " seconds");
    TABLE_ROW_48("Trigger", "NEW BAR DETECTION (IMMEDIATE LOGGING)");

    TABLE_SEPARATOR_48();

    std::string decision_reason;
    if (!csv_logger) {
        decision_reason = "CSV LOGGER UNAVAILABLE";
    } else if (should_log) {
        decision_reason = "NEW BAR DETECTED - LOGGING IMMEDIATELY";
    } else {
        decision_reason = "NO NEW BAR - SAME DATA AS PREVIOUS";
    }

    TABLE_ROW_48("DECISION", should_log ? "LOG DATA" : "SKIP LOGGING");
    TABLE_ROW_48("REASON", decision_reason);

    TABLE_FOOTER_48();
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
    TABLE_HEADER_48("ATR CALCULATION ISSUE", "Market Data Quality Warning");

    TABLE_ROW_48("Issue", "ATR calculation returned zero");
    TABLE_ROW_48("Symbol", symbol);
    TABLE_ROW_48("Impact", "Market snapshot not updated");
    TABLE_ROW_48("Reason", "Insufficient/poor quality bar data");

    TABLE_SEPARATOR_48();

    TABLE_ROW_48("STATUS", "MTH-TS DATA PRESERVED");

    TABLE_FOOTER_48();
}

void MarketDataThreadLogs::log_insufficient_data_condensed(const std::string& symbol, bool atr_zero, bool price_data_invalid, double close_price, double open_price, double high_price, double low_price, size_t bars_available, int bars_required) {
    TABLE_HEADER_48("Insufficient Data", "Waiting for Market Data Accumulation");
    
    TABLE_ROW_48("Symbol", symbol);
    
    std::string status_text = "Accumulating";
    if (atr_zero && price_data_invalid) {
        status_text = "Waiting for bars";
    } else if (atr_zero) {
        status_text = "ATR calculating";
    } else if (price_data_invalid) {
        status_text = "Price data invalid";
    }
    TABLE_ROW_48("Status", status_text);
    
    // Progress indicator: bars available / bars required
    std::string progress_text;
    if (bars_required > 0) {
        double progress_percentage = (static_cast<double>(bars_available) / static_cast<double>(bars_required)) * 100.0;
        progress_percentage = std::min(100.0, std::max(0.0, progress_percentage));
        
        std::ostringstream progress_stream;
        progress_stream << std::fixed << std::setprecision(1) << bars_available << " / " << bars_required 
                       << " (" << progress_percentage << "%)";
        progress_text = progress_stream.str();
    } else {
        progress_text = std::to_string(bars_available) + " / ?";
    }
    TABLE_ROW_48("Data Progress", progress_text);
    
    std::string atr_status = atr_zero ? "Zero (calculating)" : "Available";
    TABLE_ROW_48("ATR", atr_status);
    
    std::string price_data_status;
    if (price_data_invalid) {
        price_data_status = "Invalid (O:" + std::to_string(open_price).substr(0, 6) + 
                           " H:" + std::to_string(high_price).substr(0, 6) + 
                           " L:" + std::to_string(low_price).substr(0, 6) + 
                           " C:" + std::to_string(close_price).substr(0, 6) + ")";
    } else {
        price_data_status = "Valid";
    }
    TABLE_ROW_48("Price Data", price_data_status);
    
    std::string csv_status = (bars_available > 0) ? "Available" : "No bars";
    TABLE_ROW_48("CSV Logging", csv_status);
    
    TABLE_FOOTER_48();
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

void MarketDataThreadLogs::process_csv_logging_if_needed(const ProcessedData& computed_data, const std::vector<Bar>& historical_bars, const std::string& symbol, const TimingConfig&, const LoggingConfig& logging_config, std::chrono::steady_clock::time_point& last_bar_log_time, Bar& previous_bar) {
    
    auto csv_logger = get_logging_context()->csv_bars_logger;
    if (!csv_logger) {
        return;
    }
    
    // Check if we have valid price data - if not, throttle CSV logging decision messages
    bool has_valid_price_data = computed_data.curr.close_price > 0.0 && 
                                computed_data.curr.open_price > 0.0 &&
                                computed_data.curr.high_price > 0.0 &&
                                computed_data.curr.low_price > 0.0;
    
    auto current_time = std::chrono::steady_clock::now();
    auto time_since_last_log = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_bar_log_time).count();

    // Check if there's a new bar to log (immediate logging on new bars)
    // Compare both timestamp AND price data to detect truly new bars
    bool has_new_bar = false;
    if (!historical_bars.empty()) {
        const auto& latest_bar = historical_bars.back();
        bool timestamp_different = previous_bar.timestamp.empty() || latest_bar.timestamp != previous_bar.timestamp;
        bool price_data_different = (latest_bar.close_price != previous_bar.close_price) ||
                                   (latest_bar.open_price != previous_bar.open_price) ||
                                   (latest_bar.high_price != previous_bar.high_price) ||
                                   (latest_bar.low_price != previous_bar.low_price) ||
                                   (latest_bar.volume != previous_bar.volume);
        has_new_bar = timestamp_different || price_data_different;
    }

    // IMMEDIATE LOGGING: Log every new bar as soon as it arrives
    // For MTH-TS with 1-second bars, we must capture every single bar
    // Deduplication is handled by checking if the bar is actually new (timestamp or data changed)
    bool should_log_csv_data = has_new_bar || (last_bar_log_time == std::chrono::steady_clock::time_point{});
    
    // Throttle CSV logging DECISION messages only when waiting for data (reduce log spam)
    static std::chrono::steady_clock::time_point last_csv_decision_log_time = std::chrono::steady_clock::time_point{};
    static constexpr int csv_decision_log_interval_seconds = 5; // Log decision messages every 5 seconds when waiting
    
    bool should_log_csv_decision = true;
    if (!has_valid_price_data && historical_bars.empty()) {
        // No data available - throttle decision messages
        if (last_csv_decision_log_time != std::chrono::steady_clock::time_point{}) {
            auto time_since_last_csv_log = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_csv_decision_log_time).count();
            should_log_csv_decision = (time_since_last_csv_log >= csv_decision_log_interval_seconds);
        } else {
            last_csv_decision_log_time = current_time;
        }
    } else {
        // Data available - always log decision messages, reset throttling timer
        last_csv_decision_log_time = std::chrono::steady_clock::time_point{};
    }
    
    if (should_log_csv_decision) {
        MarketDataThreadLogs::log_csv_logging_decision(symbol, should_log_csv_data, time_since_last_log, logging_config);
        if (!has_valid_price_data && historical_bars.empty()) {
            last_csv_decision_log_time = current_time;
        }
    }
    
    if (!should_log_csv_data) {
        LOG_THREAD_CONTENT("Skipping CSV logging - no new bar detected (same timestamp and OHLCV data as previous)");
        return;
    }
    
    try {
        std::string current_timestamp = TimeUtils::get_current_human_readable_time();
        
        // Log bars directly without making API calls for quotes
        // This logs all bars in the historical_bars vector when new bars are detected
            if (!historical_bars.empty()) {
                MarketDataThreadLogs::log_historical_bars_to_csv(historical_bars, computed_data, current_timestamp, symbol);
        }
        
        last_bar_log_time = current_time;
    } catch (const std::exception& exception) {
        MarketDataThreadLogs::log_csv_logging_error(symbol, exception.what());
    } catch (...) {
        MarketDataThreadLogs::log_csv_logging_error(symbol, "Unknown error");
    }
}

