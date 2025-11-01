#ifndef TRADING_COORDINATOR_HPP
#define TRADING_COORDINATOR_HPP

#include "configs/system_config.hpp"
#include "trader/trading_logic/trading_logic.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "trader/data_structures/data_sync_structures.hpp"
#include "trader/market_data/market_data_fetcher.hpp"
#include "trader/account_management/account_manager.hpp"
#include "utils/connectivity_manager.hpp"
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>

namespace AlpacaTrader {
namespace Core {

class TradingCoordinator {
public:
    struct TradingSnapshotState {
        MarketSnapshot& market_snapshot;
        AccountSnapshot& account_snapshot;
        std::mutex& state_mutex;
        std::condition_variable& data_condition_variable;
        std::atomic<bool>& has_market_flag;
        std::atomic<bool>& has_account_flag;
        std::atomic<bool>& running_flag;
    };

    TradingCoordinator(TradingLogic& trading_logic_ref, MarketDataFetcher& data_fetcher_ref, 
                      ConnectivityManager& connectivity_manager_ref,
                      AccountManager& account_manager_ref,
                      const SystemConfig& system_config_param);
    
    void execute_trading_cycle_iteration(TradingSnapshotState& snapshot_state, 
                                         MarketDataSyncState& market_data_sync_state,
                                         double initial_equity,
                                         unsigned long loop_counter_value);
    TradingLogic& get_trading_logic_reference();
    MarketDataFetcher& get_market_data_fetcher_reference();

private:
    TradingLogic& trading_logic;
    MarketDataFetcher& data_fetcher;
    ConnectivityManager& connectivity_manager;
    AccountManager& account_manager;
    const SystemConfig& config;
    
    bool validate_connectivity_status() const;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_COORDINATOR_HPP
