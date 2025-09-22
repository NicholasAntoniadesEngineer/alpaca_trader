#ifndef ACCOUNT_DATA_THREAD_HPP
#define ACCOUNT_DATA_THREAD_HPP

#include "../configs/timing_config.hpp"
#include "../account_manager/account_manager.hpp"
#include "../utils/data_structures.hpp"
#include "../configs/component_configs.hpp"
#include <atomic>
#include <mutex>
#include <condition_variable>

struct AccountDataThread {
    const TimingConfig& timing;
    AccountManager& account_manager;
    std::mutex& state_mtx;
    std::condition_variable& data_cv;
    AccountSnapshot& account_snapshot;
    std::atomic<bool>& has_account;
    std::atomic<bool>& running;
    std::atomic<bool>* allow_fetch_ptr {nullptr};
    std::atomic<unsigned long>* iteration_counter {nullptr};

    AccountDataThread(const AccountDataThreadConfig& cfg,
                    AccountManager& accountMgr,
                    std::mutex& mtx,
                    std::condition_variable& cv,
                    AccountSnapshot& snapshot,
                    std::atomic<bool>& has_account_flag,
                    std::atomic<bool>& running_flag)
        : timing(cfg.timing), account_manager(accountMgr), state_mtx(mtx),
          data_cv(cv), account_snapshot(snapshot), has_account(has_account_flag),
          running(running_flag) {}

    void set_allow_fetch_flag(std::atomic<bool>& allow_flag) { allow_fetch_ptr = &allow_flag; }
    void set_iteration_counter(std::atomic<unsigned long>& counter) { iteration_counter = &counter; }

    // Thread entrypoint
    void operator()();
};

#endif // ACCOUNT_DATA_THREAD_HPP
