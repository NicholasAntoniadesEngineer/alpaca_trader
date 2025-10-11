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

void MarketDataLogs::log_market_data_failure_summary(const std::string& symbol, const std::string& error_type, 
                                                     const std::string& error_details, size_t bars_received, 
                                                     const std::string& log_file) {
    log_message("", log_file);
    log_message("================================================================================", log_file);
    log_message("                           MARKET DATA FAILURE SUMMARY", log_file);
    log_message("================================================================================", log_file);
    log_message("", log_file);
    
    // Symbol and error information
    log_message("+-- FAILURE DETAILS", log_file);
    log_message("|   Symbol: " + symbol, log_file);
    log_message("|   Error Type: " + error_type, log_file);
    log_message("|   Error Details: " + error_details, log_file);
    log_message("|   Bars Received: " + std::to_string(bars_received), log_file);
    log_message("|", log_file);
    
    // Possible causes and solutions
    log_message("+-- POSSIBLE CAUSES & SOLUTIONS", log_file);
    
    if (error_type == "Invalid Symbol") {
        log_message("|   • Symbol format invalid - use format: SYMBOL/USD for crypto, SYMBOL for stocks", log_file);
        log_message("|   • Check symbol exists on Alpaca platform", log_file);
        log_message("|   • Verify symbol is active and tradable", log_file);
    } else if (error_type == "No Data Available") {
        log_message("|   • Market may be closed (weekend/holiday)", log_file);
        log_message("|   • Symbol may not exist or be inactive", log_file);
        log_message("|   • API key may lack permissions for this symbol", log_file);
        log_message("|   • Check account status and subscription level", log_file);
    } else if (error_type == "Insufficient Data") {
        log_message("|   • Not enough historical data for calculations", log_file);
        log_message("|   • Symbol may be newly listed or have limited history", log_file);
        log_message("|   • Try increasing bars_to_fetch_for_calculations in config", log_file);
    } else if (error_type == "API Error") {
        log_message("|   • Check API key permissions and account status", log_file);
        log_message("|   • Verify API endpoints are correct", log_file);
        log_message("|   • Check rate limits and subscription level", log_file);
        log_message("|   • Ensure market is open for real-time data", log_file);
    } else {
        log_message("|   • Check API key permissions and account status", log_file);
        log_message("|   • Verify symbol exists and is active", log_file);
        log_message("|   • Check market hours and trading availability", log_file);
        log_message("|   • Review API endpoint configuration", log_file);
    }
    
    log_message("|", log_file);
    
    // Data source information
    log_message("+-- DATA SOURCE STATUS", log_file);
    log_message("|   • IEX FREE FEED: Limited symbol coverage, 15-min delay", log_file);
    log_message("|   • SIP PAID FEED: Requires subscription ($100+/month)", log_file);
    log_message("|   • CRYPTO FEED: Real-time data for supported crypto pairs", log_file);
    log_message("|", log_file);
    
    log_message("+-- ", log_file);
    log_message("", log_file);
}

} // namespace Logging
} // namespace AlpacaTrader
