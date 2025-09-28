#ifndef ACCOUNT_LOGS_HPP
#define ACCOUNT_LOGS_HPP

#include "configs/logging_config.hpp"
#include "core/trader/data/account_manager.hpp"

namespace AlpacaTrader {
namespace Logging {

class AccountLogs {
private:
    const LoggingConfig& logging;
    Core::AccountManager& account_manager;

public:
    AccountLogs(const LoggingConfig& logging_cfg, Core::AccountManager& account_mgr);

    void display_account_status() const;
    
    // Error logging functions
    static void log_account_api_error(const std::string& message, const std::string& log_file);
    static void log_account_parse_error(const std::string& error, const std::string& raw_response, const std::string& log_file);
    static void log_account_field_missing(const std::string& field_name, const std::string& log_file);
    static void log_account_empty_response(const std::string& log_file);
    static void log_position_parse_error(const std::string& error, const std::string& raw_response, const std::string& log_file);
    static void log_orders_parse_error(const std::string& error, const std::string& raw_response, const std::string& log_file);
    
private:
    void display_account_overview() const;
    void display_financial_summary() const;
    void display_positions() const;
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // ACCOUNT_LOGS_HPP
