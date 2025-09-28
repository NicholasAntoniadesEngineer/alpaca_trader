/**
 * Account data polling thread.
 * Maintains current account state for trading decisions.
 */
#include "account_data_thread.hpp"
#include "logging/async_logger.hpp"
#include "logging/startup_logs.hpp"
#include "../thread_logic/platform/thread_control.hpp"
#include <chrono>

using AlpacaTrader::Threads::AccountDataThread;
using AlpacaTrader::Logging::set_log_thread_tag;
using AlpacaTrader::Logging::log_message;
using AlpacaTrader::Core::AccountSnapshot;

void AlpacaTrader::Threads::AccountDataThread::operator()() {
    set_log_thread_tag("ACCOUNT");
    
    // Wait for main thread to complete priority setup
    std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_delay_ms));
    
    account_data_loop();
}

void AlpacaTrader::Threads::AccountDataThread::account_data_loop() {
    while (running.load()) {
        if (allow_fetch_ptr && !allow_fetch_ptr->load()) {
            std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
            continue;
        }

        fetch_and_update_account_data();

        if (iteration_counter) {
            iteration_counter->fetch_add(1);
        }

        std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
    }
}

void AlpacaTrader::Threads::AccountDataThread::fetch_and_update_account_data() {
    AccountSnapshot temp = account_manager.get_account_snapshot();
    {
        std::lock_guard<std::mutex> lock(state_mtx);
        account_snapshot = temp;
        has_account.store(true);
    }
    data_cv.notify_all();
}
