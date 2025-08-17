#ifndef ACCOUNT_DISPLAY_HPP
#define ACCOUNT_DISPLAY_HPP

#include "../configs/logging_config.hpp"
#include "../data/account_manager.hpp"

class AccountDisplay {
private:
    const LoggingConfig& logging;
    AccountManager& account_manager;

public:
    AccountDisplay(const LoggingConfig& loggingCfg, AccountManager& accountMgr);

    void display_account_status() const;
    
private:
    void display_account_overview() const;
    void display_financial_summary() const;
    void display_positions() const;
};

#endif // ACCOUNT_DISPLAY_HPP
