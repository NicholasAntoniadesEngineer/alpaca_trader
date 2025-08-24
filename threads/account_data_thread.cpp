/**
 * Account data polling thread.
 * Maintains current account state for trading decisions.
 */
#include "account_data_thread.hpp"
#include "../logging/async_logger.hpp"
#include "platform/thread_control.hpp"
#include <chrono>

void AccountDataThread::operator()() {
    set_log_thread_tag("ACCOUNT");
    log_message("   |  • Account data thread started: " + ThreadSystem::Platform::ThreadControl::get_thread_info(), "");
    
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
