// account_display.hpp - Handles account status presentation
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

    // Account status display and formatting (moved from AlpacaClient)
    void display_account_status() const;
    
private:
    // Helper methods for cleaner display
    void display_account_overview() const;
    void display_financial_summary() const;
    void display_positions() const;
};

#endif // ACCOUNT_DISPLAY_HPP
