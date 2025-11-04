#ifndef TRADER_THREAD_HPP
#define TRADER_THREAD_HPP

#include "configs/timing_config.hpp"
#include "trader/coordinators/trading_coordinator.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "trader/data_structures/data_sync_structures.hpp"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace AlpacaTrader {
namespace Threads {

using AlpacaTrader::Core::MarketSnapshot;
using AlpacaTrader::Core::AccountSnapshot;

struct TraderThread {
    const TimingConfig& timing;
    AlpacaTrader::Core::TradingCoordinator& trading_coordinator;
    std::mutex& state_mtx;
    std::condition_variable& data_cv;
    MarketSnapshot& market_snapshot;
    AccountSnapshot& account_snapshot;
    std::atomic<bool>& has_market;
    std::atomic<bool>& has_account;
    std::atomic<bool>& running;
    std::atomic<std::chrono::steady_clock::time_point>& market_data_timestamp;
    std::atomic<bool>& market_data_fresh;
    std::atomic<std::chrono::steady_clock::time_point>& last_order_timestamp;
    
    std::atomic<unsigned long> loop_counter{0};
    std::atomic<unsigned long>* iteration_counter{nullptr};
    std::atomic<bool>* allow_fetch_ptr{nullptr};
    double initial_equity;

    TraderThread(const TimingConfig& timing_config,
                AlpacaTrader::Core::TradingCoordinator& coordinator_ref,
                std::mutex& mtx,
                std::condition_variable& cv,
                MarketSnapshot& market_snapshot_ref,
                AccountSnapshot& account_snapshot_ref,
                std::atomic<bool>& has_market_flag,
                std::atomic<bool>& has_account_flag,
                std::atomic<bool>& running_flag,
                std::atomic<std::chrono::steady_clock::time_point>& timestamp,
                std::atomic<bool>& fresh_flag,
                std::atomic<std::chrono::steady_clock::time_point>& last_order_ref,
                double initial_equity_value)
        : timing(timing_config), trading_coordinator(coordinator_ref),
          state_mtx(mtx), data_cv(cv), market_snapshot(market_snapshot_ref), account_snapshot(account_snapshot_ref),
          has_market(has_market_flag), has_account(has_account_flag), running(running_flag),
          market_data_timestamp(timestamp), market_data_fresh(fresh_flag), last_order_timestamp(last_order_ref),
          initial_equity(initial_equity_value) {}

    // External gate: when set, task will fetch; otherwise it sleeps
    void set_allow_fetch_flag(std::atomic<bool>& allow_flag) { allow_fetch_ptr = &allow_flag; }
    
    // Set iteration counter for monitoring
    void set_iteration_counter(std::atomic<unsigned long>& counter) { iteration_counter = &counter; }

    // Thread entrypoint
    void operator()();

private:
    // Thread lifecycle management
    void execute_trading_decision_loop();
};

} // namespace Threads
} // namespace AlpacaTrader

#endif // TRADER_THREAD_HPP
