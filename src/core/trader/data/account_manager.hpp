#ifndef ACCOUNT_MANAGER_HPP
#define ACCOUNT_MANAGER_HPP

#include "configs/api_config.hpp"
#include "configs/logging_config.hpp"
#include "configs/strategy_config.hpp"
#include "core/threads/thread_register.hpp"
#include "data_structures.hpp"
#include <string>
#include <chrono>
#include <mutex>

namespace AlpacaTrader {
namespace Core {

using AccountManagerConfig = AlpacaTrader::Config::AccountManagerConfig;

class AccountManager {
public:
    // Account information for display purposes
    struct AccountInfo {
        std::string account_number;
        std::string status;
        std::string currency;
        bool pattern_day_trader;
        std::string trading_blocked_reason;
        std::string transfers_blocked_reason;
        std::string account_blocked_reason;
        std::string created_at;
        double equity;
        double last_equity;
        double long_market_value;
        double short_market_value;
        double cash;
        double buying_power;
        double initial_margin;
        double maintenance_margin;
        double sma;
        double day_trade_count;
        double regt_buying_power;
        double daytrading_buying_power;
    };

    explicit AccountManager(const AccountManagerConfig& cfg);

    double fetch_account_equity() const;
    double fetch_buying_power() const;
    PositionDetails fetch_position_details(const SymbolRequest& req_sym) const;
    int fetch_open_orders_count(const SymbolRequest& req_sym) const;

    AccountSnapshot fetch_account_snapshot() const;
    std::pair<AccountInfo, AccountSnapshot> fetch_account_data_bundled() const;
    AccountInfo fetch_account_info() const;

private:
    const AlpacaTrader::Config::ApiConfig& api;
    const LoggingConfig& logging;
    const StrategyConfig& strategy;  // Now contains target settings
    int cache_duration_seconds;
    
    // Caching for rate limit optimization
    mutable std::pair<AccountInfo, AccountSnapshot> cached_data;
    mutable std::chrono::steady_clock::time_point last_cache_time;
    mutable std::mutex cache_mutex;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // ACCOUNT_MANAGER_HPP
