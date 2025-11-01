#include "market_data_logs.hpp"
#include "core/logging/logger/async_logger.hpp"
#include "core/trader/data_structures/data_structures.hpp"
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

void MarketDataLogs::log_current_positions_table(int position_quantity, double current_value, double unrealized_pl, double exposure_pct, int open_orders, const std::string& log_file, const std::string& position_long_string, const std::string& position_short_string) {
    log_message("+-- CURRENT POSITIONS", log_file);
    
    if (position_quantity == 0) {
        log_message("|   No positions held", log_file);
    } else {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        
        std::string side = (position_quantity > 0) ? position_long_string : position_short_string;
        oss << "|   Position: " << side << " " << std::abs(position_quantity) << " shares";
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

void MarketDataLogs::log_position_data_and_warnings(int position_quantity, double current_value, double unrealized_pl, double exposure_pct, int open_orders, const std::string& log_file, const std::string& position_long_string, const std::string& position_short_string) {
    log_current_positions_table(position_quantity, current_value, unrealized_pl, exposure_pct, open_orders, log_file, position_long_string, position_short_string);
    
    if (position_quantity != 0 && open_orders == 0) {
        log_market_data_result_table("Missing bracket order warning", true, 0, log_file);
    }
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
    log_message("                        MULTI-API MARKET DATA FAILURE", log_file);
    log_message("================================================================================", log_file);
    log_message("", log_file);
    
    // Symbol and error information
    log_message("FAILURE DETAILS:", log_file);
    log_message("  Symbol: " + symbol, log_file);
    log_message("  Error Type: " + error_type, log_file);
    log_message("  Error Details: " + error_details, log_file);
    log_message("  Bars Received: " + std::to_string(bars_received), log_file);
    log_message("", log_file);
    
    // Provider-specific solutions
    log_message("PROVIDER-SPECIFIC SOLUTIONS:", log_file);
    
    if (error_type == "Invalid Symbol") {
        log_message("  • STOCKS: Use format 'SYMBOL' (Alpaca Trading/Stocks providers)", log_file);
        log_message("  • CRYPTO: Use format 'SYMBOL/PAIR' (Polygon Crypto provider)", log_file);
        log_message("  • Verify symbol exists on the configured provider", log_file);
    } else if (error_type == "No Data Available") {
        log_message("  • Check if market is open for the asset class", log_file);
        log_message("  • STOCKS: NYSE/NASDAQ hours (9:30-16:00 ET, Mon-Fri)", log_file);
        log_message("  • CRYPTO: 24/7 availability (check Polygon.io status)", log_file);
        log_message("  • Verify API provider has data for this symbol", log_file);
    } else if (error_type == "Insufficient Data") {
        log_message("  • Not enough historical data for technical analysis", log_file);
        log_message("  • Try reducing bars_to_fetch_for_calculations in config", log_file);
        log_message("  • Symbol may be newly listed or have limited history", log_file);
    } else if (error_type == "API Error") {
        log_message("  • Check API provider configuration in api_endpoints_config.csv", log_file);
        log_message("  • Verify API keys are valid and have required permissions", log_file);
        log_message("  • Check rate limits for the specific provider", log_file);
        log_message("  • Ensure provider endpoints are correctly configured", log_file);
    } else {
        log_message("  • Review multi-API configuration in api_endpoints_config.csv", log_file);
        log_message("  • Verify trading_mode.mode matches symbol type (stocks/crypto)", log_file);
        log_message("  • Check provider-specific API key permissions", log_file);
        log_message("  • Ensure correct provider is selected for symbol type", log_file);
    }
    
    log_message("", log_file);
    
    // Multi-API provider status
    log_message("CONFIGURED API PROVIDERS:", log_file);
    log_message("  • ALPACA TRADING: Account, orders, positions", log_file);
    log_message("  • ALPACA STOCKS: Market data for stocks (IEX feed)", log_file);
    log_message("  • POLYGON CRYPTO: Real-time crypto data (if configured)", log_file);
    log_message("", log_file);
    
    log_message("Check api_endpoints_config.csv for provider configuration", log_file);
    log_message("================================================================================", log_file);
    log_message("", log_file);
}

} // namespace Logging
} // namespace AlpacaTrader
