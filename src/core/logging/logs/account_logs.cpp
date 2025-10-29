// account_logger.cpp
#include "account_logs.hpp"
#include "core/logging/logger/async_logger.hpp"
#include "core/trader/data/data_structures.hpp"
#include <string>
#include <iomanip>
#include <sstream>

namespace AlpacaTrader {
namespace Logging {

AccountLogs::AccountLogs(const LoggingConfig& logging_cfg, Core::AccountManager& account_mgr)
    : logging(logging_cfg), account_manager(account_mgr) {}

void AccountLogs::display_account_status() const {
    log_message("", logging.log_file);
    log_message("================================================================================", logging.log_file);
    log_message("                              ACCOUNT STATUS SUMMARY", logging.log_file);
    log_message("================================================================================", logging.log_file);

    display_account_overview();
    display_financial_summary();
    display_positions();
    
    log_message("-------------------------------------------------------------------------------", logging.log_file);
    log_message("", logging.log_file);
}

void AccountLogs::display_account_overview() const {
    auto [account_info, snapshot] = account_manager.fetch_account_data_bundled();
    
    log_message("+-- ACCOUNT OVERVIEW", logging.log_file);
    if (!account_info.account_number.empty()) {
        log_message("|   Account Number: " + account_info.account_number, logging.log_file);
    }
    if (!account_info.status.empty()) {
        log_message("|   Status: " + account_info.status, logging.log_file);
    }
    if (!account_info.currency.empty()) {
        log_message("|   Currency: " + account_info.currency, logging.log_file);
    }
    log_message("|   Pattern Day Trader: " + std::string(account_info.pattern_day_trader ? "YES" : "NO"), logging.log_file);
    
    if (!account_info.trading_blocked_reason.empty()) {
        log_message("|   Trading Blocked: " + account_info.trading_blocked_reason, logging.log_file);
    }
    if (!account_info.transfers_blocked_reason.empty()) {
        log_message("|   Transfers Blocked: " + account_info.transfers_blocked_reason, logging.log_file);
    }
    if (!account_info.account_blocked_reason.empty()) {
        log_message("|   Account Blocked: " + account_info.account_blocked_reason, logging.log_file);
    }
    if (!account_info.created_at.empty()) {
        log_message("|   Created: " + account_info.created_at, logging.log_file);
    }
    log_message("|", logging.log_file);
}

void AccountLogs::display_financial_summary() const {
    auto [account_info, snapshot] = account_manager.fetch_account_data_bundled();
    
    log_message("+-- FINANCIAL SUMMARY", logging.log_file);
    
    // Helper function to format currency
    auto format_currency = [](double value) -> std::string {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << "$" << value;
        return oss.str();
    };
    
    log_message("|   Equity: " + format_currency(account_info.equity), logging.log_file);
    log_message("|   Last Equity: " + format_currency(account_info.last_equity), logging.log_file);
    log_message("|   Cash: " + format_currency(account_info.cash), logging.log_file);
    log_message("|   Buying Power: " + format_currency(account_info.buying_power), logging.log_file);
    log_message("|   Long Market Value: " + format_currency(account_info.long_market_value), logging.log_file);
    log_message("|   Short Market Value: " + format_currency(account_info.short_market_value), logging.log_file);
    log_message("|   Initial Margin: " + format_currency(account_info.initial_margin), logging.log_file);
    log_message("|   Maintenance Margin: " + format_currency(account_info.maintenance_margin), logging.log_file);
    log_message("|   SMA: " + format_currency(account_info.sma), logging.log_file);
    log_message("|   Day Trade Count: " + std::to_string(static_cast<int>(account_info.day_trade_count)), logging.log_file);
    log_message("|   RegT Buying Power: " + format_currency(account_info.regt_buying_power), logging.log_file);
    log_message("|   Day Trading Buying Power: " + format_currency(account_info.daytrading_buying_power), logging.log_file);
    log_message("|", logging.log_file);
}

void AccountLogs::display_positions() const {
    auto [account_info, snapshot] = account_manager.fetch_account_data_bundled();
    
    log_message("+-- CURRENT POSITIONS", logging.log_file);
    
    if (snapshot.pos_details.qty == 0) {
        log_message("|   No positions held", logging.log_file);
    } else {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        
        std::string side = (snapshot.pos_details.qty > 0) ? POSITION_LONG : POSITION_SHORT;
        oss << "|   Position: " << side << " " << std::abs(snapshot.pos_details.qty) << " shares";
        log_message(oss.str(), logging.log_file);
        
        oss.str("");
        oss << "|   Current Value: $" << snapshot.pos_details.current_value;
        log_message(oss.str(), logging.log_file);
        
        oss.str("");
        oss << "|   Unrealized P/L: $" << snapshot.pos_details.unrealized_pl;
        log_message(oss.str(), logging.log_file);
        
        oss.str("");
        oss << "|   Exposure: " << std::setprecision(1) << snapshot.exposure_pct << "%";
        log_message(oss.str(), logging.log_file);
    }
    
    if (snapshot.open_orders > 0) {
        log_message("|   Open Orders: " + std::to_string(snapshot.open_orders), logging.log_file);
    }
    
    log_message("|", logging.log_file);
}

// Error logging functions
void AccountLogs::log_account_api_error(const std::string& message, const std::string& log_file) {
    log_message("ERROR: Account API error: " + message, log_file);
}

void AccountLogs::log_account_parse_error(const std::string& error, const std::string& raw_response, const std::string& log_file) {
    log_message("ERROR: Failed to parse account data: " + error + "; raw: " + raw_response, log_file);
}

void AccountLogs::log_account_field_missing(const std::string& field_name, const std::string& log_file) {
    log_message("ERROR: " + field_name + " field missing in account response", log_file);
}

void AccountLogs::log_account_empty_response(const std::string& log_file) {
    log_message("ERROR: Unable to retrieve account information (empty response)", log_file);
}

void AccountLogs::log_position_parse_error(const std::string& error, const std::string& raw_response, const std::string& log_file) {
    log_message("Error parsing position details: " + error, log_file);
    log_message("Raw position response: " + raw_response, log_file);
}

void AccountLogs::log_orders_parse_error(const std::string& error, const std::string& raw_response, const std::string& log_file) {
    log_message("Error parsing open orders: " + error, log_file);
    log_message("Raw orders response: " + raw_response, log_file);
}

void AccountLogs::log_position_empty_response(const std::string& log_file) {
    log_message("INFO: Empty response when fetching position details", log_file);
}

void AccountLogs::log_position_not_found(const std::string& symbol, const std::string& log_file) {
    log_message("INFO: Position not found for symbol " + symbol + " (no position held)", log_file);
}

} // namespace Logging
} // namespace AlpacaTrader