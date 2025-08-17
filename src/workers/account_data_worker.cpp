// AccountDataWorker.cpp
#include "workers/account_data_worker.hpp"
#include "utils/async_logger.hpp"
#include <chrono>

AccountDataWorker::AccountDataWorker(const TimingConfig& timingCfg,
                                     AccountManager& accountMgr,
                                     std::mutex& mtx,
                                     std::condition_variable& cv,
                                     AccountSnapshot& snapshot,
                                     std::atomic<bool>& has_account_flag,
                                     std::atomic<bool>& running_flag)
    : timing(timingCfg), account_manager(accountMgr), state_mtx(mtx), data_cv(cv), account_snapshot(snapshot), has_account(has_account_flag), running(running_flag) {}

void AccountDataWorker::start() {
    worker = std::thread([this](){
        set_log_thread_tag("ACCOUNT");
        while (running.load()) {
            if (allow_fetch_ptr && !allow_fetch_ptr->load()) {
                std::this_thread::sleep_for(std::chrono::seconds(timing.account_poll_sec));
                continue;
            }
            AccountSnapshot temp = account_manager.get_account_snapshot();
            {
                std::lock_guard<std::mutex> lock(state_mtx);
                account_snapshot = temp;
                has_account.store(true);
            }
            data_cv.notify_all();
            std::this_thread::sleep_for(std::chrono::seconds(timing.account_poll_sec));
        }
    });
}

void AccountDataWorker::stop() { running.store(false); data_cv.notify_all(); }
void AccountDataWorker::join() { if (worker.joinable()) worker.join(); }
