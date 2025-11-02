#ifndef TRADING_COORDINATOR_HPP
#define TRADING_COORDINATOR_HPP

#include "configs/system_config.hpp"
#include "trader/trading_logic/trading_logic.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "trader/data_structures/data_sync_structures.hpp"
#include "trader/market_data/market_data_manager.hpp"
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

    TradingCoordinator(TradingLogic& trading_logic_ref, MarketDataManager& market_data_manager_ref, 
                      ConnectivityManager& connectivity_manager_ref,
                      AccountManager& account_manager_ref,
                      const SystemConfig& system_config_param);
    
    void execute_trading_cycle_iteration(TradingSnapshotState& snapshot_state, 
                                         MarketDataSyncState& market_data_sync_state,
                                         double initial_equity,
                                         unsigned long loop_counter_value);
    
    // Process a complete trading cycle iteration (creates state structures and executes)
    void process_trading_cycle_iteration(MarketSnapshot& market_snapshot,
                                         AccountSnapshot& account_snapshot,
                                         std::mutex& state_mtx,
                                         std::condition_variable& data_cv,
                                         std::atomic<bool>& has_market,
                                         std::atomic<bool>& has_account,
                                         std::atomic<bool>& running,
                                         std::atomic<std::chrono::steady_clock::time_point>& market_data_timestamp,
                                         std::atomic<bool>& market_data_fresh,
                                         std::atomic<std::chrono::steady_clock::time_point>& last_order_timestamp,
                                         std::atomic<bool>* allow_fetch_ptr,
                                         double initial_equity,
                                         std::atomic<unsigned long>& loop_counter);
    
    // Countdown and sleep between cycles
    void countdown_to_next_cycle(std::atomic<bool>& running, int poll_interval_sec, int countdown_refresh_interval_sec);
    
    MarketDataManager& get_market_data_manager_reference();

private:
    TradingLogic& trading_logic;
    MarketDataManager& market_data_manager;
    ConnectivityManager& connectivity_manager;
    AccountManager& account_manager;
    const SystemConfig& config;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_COORDINATOR_HPP
