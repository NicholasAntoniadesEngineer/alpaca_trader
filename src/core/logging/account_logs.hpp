#ifndef ACCOUNT_LOGS_HPP
#define ACCOUNT_LOGS_HPP

#include "configs/logging_config.hpp"
#include "core/trader/account_manager.hpp"

namespace AlpacaTrader {
namespace Logging {

class AccountLogs {
private:
    const LoggingConfig& logging;
    Core::AccountManager& account_manager;

public:
    AccountLogs(const LoggingConfig& logging_cfg, Core::AccountManager& account_mgr);

    void display_account_status() const;
    
private:
    void display_account_overview() const;
    void display_financial_summary() const;
    void display_positions() const;
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // ACCOUNT_LOGS_HPP
