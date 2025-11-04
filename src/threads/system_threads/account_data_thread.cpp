/**
 * Account data polling thread.
 * Maintains current account state for trading decisions.
 */
#include "account_data_thread.hpp"
#include "logging/logs/account_data_thread_logs.hpp"
#include "logging/logger/async_logger.hpp"
#include "threads/thread_logic/platform/thread_control.hpp"
#include <chrono>

using namespace AlpacaTrader::Threads;
using namespace AlpacaTrader::Logging;
using namespace AlpacaTrader::Core;

void AccountDataThread::operator()() {
    set_log_thread_tag("ACCOUNT");
    
    try {
        // Wait for main thread to complete priority setup
        std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_sequence_delay_milliseconds));
        
        // Start the account data collection loop
        execute_account_data_collection_loop();
    } catch (const std::exception& exception_error) {
        AccountDataThreadLogs::log_thread_exception(exception_error.what());
    } catch (...) {
        AccountDataThreadLogs::log_thread_exception("Unknown error");
    }
}

void AccountDataThread::execute_account_data_collection_loop() {
    try {
        while (running.load()) {
            try {
                if (!AccountDataThreadLogs::is_fetch_allowed(allow_fetch_ptr)) {
                    std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
                    continue;
                }

                account_data_coordinator.fetch_and_update_account_data(account_snapshot, state_mtx, data_cv, has_account);

                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }

                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
                
            } catch (const std::exception& exception_error) {
                AccountDataThreadLogs::log_thread_loop_exception(exception_error.what());
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
            } catch (...) {
                AccountDataThreadLogs::log_thread_loop_exception("Unknown error");
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
            }
        }
    } catch (const std::exception& exception_error) {
        AccountDataThreadLogs::log_thread_exception(exception_error.what());
    } catch (...) {
        AccountDataThreadLogs::log_thread_exception("Unknown error");
    }
}
