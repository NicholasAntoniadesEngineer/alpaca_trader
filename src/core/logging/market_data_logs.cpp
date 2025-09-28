#include "market_data_logs.hpp"
#include "async_logger.hpp"
#include "core/trader/data/data_structures.hpp"
#include <iomanip>
#include <sstream>

namespace AlpacaTrader {
namespace Logging {

void MarketDataLogs::log_market_data_fetch_table(const std::string& symbol, const std::string& log_file) {
    log_message("", log_file);
    log_message("================================================================================", log_file);
    log_message("                              MARKET DATA FETCH - " + symbol, log_file);
    log_message("================================================================================", log_file);
    log_message("", log_file);
}

void MarketDataLogs::log_market_data_attempt_table(const std::string& description, const std::string& log_file) {
    log_message("+-- " + description, log_file);
}

void MarketDataLogs::log_market_data_result_table(const std::string& description, bool success, size_t bar_count, const std::string& log_file) {
    std::string status = success ? "SUCCESS" : "FAILED";
    std::string icon = success ? "✓" : "✗";
    
    log_message("|   " + icon + " " + description, log_file);
    if (bar_count > 0) {
        log_message("|   Data Points: " + std::to_string(bar_count), log_file);
    }
    log_message("|   Status: " + status, log_file);
    log_message("|", log_file);
}

void MarketDataLogs::log_current_positions_table(int qty, double current_value, double unrealized_pl, double exposure_pct, int open_orders, const std::string& log_file) {
    log_message("+-- CURRENT POSITIONS", log_file);
    
    if (qty == 0) {
        log_message("|   No positions held", log_file);
    } else {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        
        std::string side = (qty > 0) ? POSITION_LONG : POSITION_SHORT;
        oss << "|   Position: " << side << " " << std::abs(qty) << " shares";
        log_message(oss.str(), log_file);
        
        oss.str("");
        oss << "|   Current Value: $" << current_value;
        log_message(oss.str(), log_file);
        
        oss.str("");
        oss << "|   Unrealized P/L: $" << unrealized_pl;
        log_message(oss.str(), log_file);
        
        oss.str("");
        oss << "|   Exposure: " << std::setprecision(1) << exposure_pct << "%";
        log_message(oss.str(), log_file);
    }
    
    if (open_orders > 0) {
        log_message("|   Open Orders: " + std::to_string(open_orders), log_file);
    }
    
    log_message("|", log_file);
}

void MarketDataLogs::log_sync_state_error(const std::string& error_message, const std::string& log_file) {
    log_message("ERROR: " + error_message, log_file);
}

void MarketDataLogs::log_data_timeout(const std::string& log_file) {
    log_message("ERROR: Timeout waiting for fresh data", log_file);
}

void MarketDataLogs::log_data_available(const std::string& log_file) {
    log_message("INFO: Fresh data available", log_file);
}

void MarketDataLogs::log_data_exception(const std::string& error_message, const std::string& log_file) {
    log_message("ERROR: Exception in market data processing: " + error_message, log_file);
}

} // namespace Logging
} // namespace AlpacaTrader
