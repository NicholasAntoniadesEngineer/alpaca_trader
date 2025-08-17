// account_data_worker.hpp
#ifndef ACCOUNT_DATA_WORKER_HPP
#define ACCOUNT_DATA_WORKER_HPP

#include "../configs/timing_config.hpp"
#include "../data/account_manager.hpp"
#include "../data/data_structures.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

class AccountDataWorker {
private:
    const TimingConfig& timing;
    AccountManager& account_manager;
    std::mutex& state_mtx;
    std::condition_variable& data_cv;
    AccountSnapshot& account_snapshot;
    std::atomic<bool>& has_account;
    std::atomic<bool>& running;
    std::atomic<bool>* allow_fetch_ptr {nullptr};
    std::thread worker;

public:
    AccountDataWorker(const TimingConfig& timingCfg,
                      AccountManager& accountMgr,
                      std::mutex& mtx,
                      std::condition_variable& cv,
                      AccountSnapshot& snapshot,
                      std::atomic<bool>& has_account_flag,
                      std::atomic<bool>& running_flag);

    void start();
    void stop();
    void join();

    void set_allow_fetch_flag(std::atomic<bool>& allow_flag) { allow_fetch_ptr = &allow_flag; }
};

#endif // ACCOUNT_DATA_WORKER_HPP
