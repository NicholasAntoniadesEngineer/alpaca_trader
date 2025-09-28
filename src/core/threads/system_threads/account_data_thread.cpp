/**
 * Account data polling thread.
 * Maintains current account state for trading decisions.
 */
#include "account_data_thread.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/startup_logs.hpp"
#include "../thread_logic/platform/thread_control.hpp"
#include <chrono>

using AlpacaTrader::Threads::AccountDataThread;
using AlpacaTrader::Logging::set_log_thread_tag;
using AlpacaTrader::Logging::log_message;
using AlpacaTrader::Core::AccountSnapshot;

void AlpacaTrader::Threads::AccountDataThread::operator()() {
    set_log_thread_tag("ACCOUNT");
    
    try {
        // Wait for main thread to complete priority setup
        std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_delay_ms));
        
        account_data_loop();
    } catch (const std::exception& e) {
        log_message("AccountDataThread exception: " + std::string(e.what()), "trading_system.log");
    } catch (...) {
        log_message("AccountDataThread unknown exception", "trading_system.log");
    }
}

void AlpacaTrader::Threads::AccountDataThread::account_data_loop() {
    try {
        while (running.load()) {
            try {
                if (allow_fetch_ptr && !allow_fetch_ptr->load()) {
                    std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
                    continue;
                }

                fetch_and_update_account_data();

                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }

                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
            } catch (const std::exception& e) {
                log_message("AccountDataThread loop iteration exception: " + std::string(e.what()), "trading_system.log");
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
            } catch (...) {
                log_message("AccountDataThread loop iteration unknown exception", "trading_system.log");
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
            }
        }
    } catch (const std::exception& e) {
        log_message("AccountDataThread account_data_loop exception: " + std::string(e.what()), "trading_system.log");
    } catch (...) {
        log_message("AccountDataThread account_data_loop unknown exception", "trading_system.log");
    }
}

void AlpacaTrader::Threads::AccountDataThread::fetch_and_update_account_data() {
    AccountSnapshot temp = account_manager.fetch_account_snapshot();
    {
        std::lock_guard<std::mutex> lock(state_mtx);
        account_snapshot = temp;
        has_account.store(true);
    }
    data_cv.notify_all();
}
