// AccountDataTask.cpp
#include "workers/account_data_worker.hpp"
#include "utils/async_logger.hpp"
#include "utils/thread_utils.hpp"
#include <chrono>

void AccountDataTask::operator()() {
    set_log_thread_tag("ACCOUNT");
    log_message("   |  • Account data thread started: " + ThreadUtils::get_thread_info(), "");
    
    // Wait for main thread to complete priority setup
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
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
}
