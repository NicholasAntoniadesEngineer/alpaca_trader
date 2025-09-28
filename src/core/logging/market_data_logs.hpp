#ifndef MARKET_DATA_LOGS_HPP
#define MARKET_DATA_LOGS_HPP

#include "configs/logging_config.hpp"
#include <string>

namespace AlpacaTrader {
namespace Logging {

class MarketDataLogs {
public:
    // Market data fetch logging
    static void log_market_data_fetch_table(const std::string& symbol, const std::string& log_file);
    static void log_market_data_attempt_table(const std::string& description, const std::string& log_file);
    static void log_market_data_result_table(const std::string& description, bool success, size_t bar_count, const std::string& log_file);
    
    // Position logging
    static void log_current_positions_table(int qty, double current_value, double unrealized_pl, double exposure_pct, int open_orders, const std::string& log_file);
    
    // Data synchronization logging
    static void log_sync_state_error(const std::string& error_message, const std::string& log_file);
    static void log_data_timeout(const std::string& log_file);
    static void log_data_available(const std::string& log_file);
    static void log_data_exception(const std::string& error_message, const std::string& log_file);
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // MARKET_DATA_LOGS_HPP
