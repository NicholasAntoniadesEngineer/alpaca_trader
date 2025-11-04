#ifndef MARKET_DATA_THREAD_LOGS_HPP
#define MARKET_DATA_THREAD_LOGS_HPP

#include "configs/system_config.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "trader/market_data/market_data_validator.hpp"
#include "api/general/api_manager.hpp"
#include <string>
#include <atomic>
#include <chrono>

using AlpacaTrader::Config::SystemConfig;
using AlpacaTrader::Core::QuoteData;
using AlpacaTrader::Core::ProcessedData;
using AlpacaTrader::Core::Bar;
using AlpacaTrader::Core::MarketDataValidator;
using AlpacaTrader::API::ApiManager;

namespace AlpacaTrader {
namespace Logging {

class MarketDataThreadLogs {
public:
    // Thread lifecycle logging
    static void log_thread_startup(const SystemConfig& config);
    static void log_thread_exception(const std::string& error_message);
    static void log_thread_loop_exception(const std::string& error_message);
    
    // Market data processing logging
    static void log_market_data_fetch_start(const std::string& symbol, int bars_requested);
    static void log_market_data_fetch_result(const std::string& symbol, size_t bars_received);
    static void log_insufficient_bars(const std::string& symbol, size_t bars_received, int bars_required);
    static void log_atr_calculation_result(const std::string& symbol, double atr_value, double current_price);
    static void log_market_snapshot_update(const std::string& symbol);
    
    // Quote data processing logging
    static void log_quote_fetch_start(const std::string& symbol);
    static void log_quote_fetch_result(const std::string& symbol, double mid_price, const std::string& timestamp);
    static void log_quote_freshness_check(const std::string& symbol, bool is_fresh, int age_seconds);
    static void log_stale_quote_warning(const std::string& symbol, int age_seconds);
    
    // CSV logging operations
    static void log_csv_logging_decision(const std::string& symbol, bool should_log, int time_since_last_log);
    static void log_csv_quote_logging(const std::string& symbol, double mid_price);
    static void log_csv_bar_logging(const std::string& symbol, size_t bars_count);
    static void log_csv_logging_error(const std::string& symbol, const std::string& error_message);
    static void log_fresh_quote_data_to_csv(const QuoteData& quote_data, const ProcessedData& processed_data, const std::string& timestamp);
    static void log_historical_bars_to_csv(const std::vector<Bar>& historical_bars, const ProcessedData& processed_data, const std::string& timestamp, const std::string& symbol);
    
    // Data validation logging
    static void log_zero_atr_warning(const std::string& symbol);
    static void log_duplicate_bar_skipped(const std::string& symbol, const std::string& bar_timestamp);
    static void log_insufficient_data_condensed(const std::string& symbol, bool atr_zero, bool price_data_invalid, double close_price, double open_price, double high_price, double low_price, size_t bars_available);
    
    // Utility functions
    static bool is_fetch_allowed(const std::atomic<bool>* allow_fetch_ptr);
    static void process_csv_logging_if_needed(const ProcessedData& computed_data, const std::vector<Bar>& historical_bars, MarketDataValidator& validator, const std::string& symbol, const TimingConfig& timing, ApiManager& api_manager, std::chrono::steady_clock::time_point& last_bar_log_time, Bar& previous_bar);
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // MARKET_DATA_THREAD_LOGS_HPP
