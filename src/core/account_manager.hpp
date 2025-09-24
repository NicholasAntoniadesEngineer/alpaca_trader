#ifndef ACCOUNT_MANAGER_HPP
#define ACCOUNT_MANAGER_HPP

#include "configs/api_config.hpp"
#include "configs/logging_config.hpp"
#include "configs/target_config.hpp"
#include "configs/component_configs.hpp"
#include "data_structures.hpp"
#include <string>
#include <chrono>
#include <mutex>

namespace AlpacaTrader {
namespace Core {

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

    // Account data operations (moved from AlpacaClient)
    double get_equity() const;
    double get_buying_power() const;
    PositionDetails get_position_details(const SymbolRequest& req_sym) const;
    int get_open_orders_count(const SymbolRequest& req_sym) const;
    
    // Account data aggregation
    AccountSnapshot get_account_snapshot() const;
    
    // Optimized bundled data retrieval (reduces API calls)
    std::pair<AccountInfo, AccountSnapshot> get_account_data_bundled() const;
    
    AccountInfo get_account_info() const;

private:
    const ApiConfig& api;
    const LoggingConfig& logging;
    const TargetConfig& target;
    int cache_duration_seconds;
    
    // Caching for rate limit optimization
    mutable std::pair<AccountInfo, AccountSnapshot> cached_data;
    mutable std::chrono::steady_clock::time_point last_cache_time;
    mutable std::mutex cache_mutex;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // ACCOUNT_MANAGER_HPP
