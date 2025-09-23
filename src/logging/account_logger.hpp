#ifndef ACCOUNT_LOGGER_HPP
#define ACCOUNT_LOGGER_HPP

#include "../configs/logging_config.hpp"
#include "../core/account_manager.hpp"

class AccountLogger {
private:
    const LoggingConfig& logging;
    AccountManager& account_manager;

public:
    AccountLogger(const LoggingConfig& loggingCfg, AccountManager& accountMgr);

    void display_account_status() const;
    
private:
    void display_account_overview() const;
    void display_financial_summary() const;
    void display_positions() const;
};

#endif // ACCOUNT_LOGGER_HPP
