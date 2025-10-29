/**
 * Account data polling thread.
 * Maintains current account state for trading decisions.
 */
#include "account_data_thread.hpp"
#include "core/logging/logger/async_logger.hpp"
#include "core/logging/logs/startup_logs.hpp"
#include "core/threads/thread_logic/platform/thread_control.hpp"
#include <chrono>

// Using declarations for cleaner code
using namespace AlpacaTrader::Threads;
using namespace AlpacaTrader::Logging;
using namespace AlpacaTrader::Core;

// ========================================================================
// THREAD LIFECYCLE MANAGEMENT
// ========================================================================

void AccountDataThread::operator()() {
    set_log_thread_tag("ACCOUNT");
    
    try {
        // Wait for main thread to complete priority setup
        std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_sequence_delay_milliseconds));
        
        // Start the account data collection loop
        execute_account_data_collection_loop();
    } catch (const std::exception& exception) {
        log_message("AccountDataThread exception: " + std::string(exception.what()), "trading_system.log");
    } catch (...) {
        log_message("AccountDataThread unknown exception", "trading_system.log");
    }
}

void AccountDataThread::execute_account_data_collection_loop() {
    try {
        while (running.load()) {
            try {
                if (!is_fetch_allowed()) {
                    std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
                    continue;
                }

                fetch_and_update_account_data();

                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }

                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
            } catch (const std::exception& exception) {
                log_message("AccountDataThread loop iteration exception: " + std::string(exception.what()), "trading_system.log");
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
            } catch (...) {
                log_message("AccountDataThread loop iteration unknown exception", "trading_system.log");
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
            }
        }
    } catch (const std::exception& exception) {
        log_message("AccountDataThread account_data_loop exception: " + std::string(exception.what()), "trading_system.log");
    } catch (...) {
        log_message("AccountDataThread account_data_loop unknown exception", "trading_system.log");
    }
}

// ========================================================================
// ACCOUNT DATA PROCESSING
// ========================================================================

void AccountDataThread::fetch_and_update_account_data() {
    AccountSnapshot temp_snapshot = account_manager.fetch_account_snapshot();
    {
        std::lock_guard<std::mutex> state_lock(state_mtx);
        account_snapshot = temp_snapshot;
        has_account.store(true);
    }
    data_cv.notify_all();
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

bool AccountDataThread::is_fetch_allowed() const {
    return allow_fetch_ptr && allow_fetch_ptr->load();
}
