#ifndef ACCOUNT_DATA_THREAD_HPP
#define ACCOUNT_DATA_THREAD_HPP

#include "configs/timing_config.hpp"
#include "core/threads/thread_register.hpp"
#include "core/trader/coordinators/account_data_coordinator.hpp"
#include "core/trader/data_structures/data_structures.hpp"
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace AlpacaTrader {
namespace Threads {

using AccountDataThreadConfig = AlpacaTrader::Config::AccountDataThreadConfig;

struct AccountDataThread {
    const TimingConfig& timing;
    AlpacaTrader::Core::AccountDataCoordinator& account_data_coordinator;
    std::mutex& state_mtx;
    std::condition_variable& data_cv;
    AlpacaTrader::Core::AccountSnapshot& account_snapshot;
    std::atomic<bool>& has_account;
    std::atomic<bool>& running;
    std::atomic<bool>* allow_fetch_ptr {nullptr};
    std::atomic<unsigned long>* iteration_counter {nullptr};

    AccountDataThread(const AccountDataThreadConfig& cfg,
                    AlpacaTrader::Core::AccountDataCoordinator& coordinator_ref,
                    std::mutex& mtx,
                    std::condition_variable& cv,
                    AlpacaTrader::Core::AccountSnapshot& snapshot,
                    std::atomic<bool>& has_account_flag,
                    std::atomic<bool>& running_flag)
        : timing(cfg.timing), account_data_coordinator(coordinator_ref), state_mtx(mtx),
          data_cv(cv), account_snapshot(snapshot), has_account(has_account_flag),
          running(running_flag) {}

    void set_allow_fetch_flag(std::atomic<bool>& allow_flag) { allow_fetch_ptr = &allow_flag; }
    void set_iteration_counter(std::atomic<unsigned long>& counter) { iteration_counter = &counter; }

    // Thread entrypoint
    void operator()();

private:
    // Main business logic methods
    void execute_account_data_collection_loop();
    void fetch_and_update_account_data();
    
    // Utility functions
    bool is_fetch_allowed() const;
};

} // namespace Threads
} // namespace AlpacaTrader

#endif // ACCOUNT_DATA_THREAD_HPP
