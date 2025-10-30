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
    } catch (const std::exception& exception_error) {
        log_message("AccountDataThread exception: " + std::string(exception_error.what()), "trading_system.log");
    } catch (...) {
        log_message("AccountDataThread unknown exception", "trading_system.log");
    }
}

void AccountDataThread::execute_account_data_collection_loop() {
    try {
        log_message("AccountDataThread entered execute_account_data_collection_loop", "trading_system.log");
        while (running.load()) {
            try {
                log_message("AccountDataThread before is_fetch_allowed", "trading_system.log");
                if (!is_fetch_allowed()) {
                    std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
                    continue;
                }

                log_message("AccountDataThread before fetch_and_update_account_data", "trading_system.log");
                fetch_and_update_account_data();

                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }

                log_message("AccountDataThread iteration complete - sleeping", "trading_system.log");
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
            } catch (const std::exception& exception_error) {
                log_message("AccountDataThread loop iteration exception: " + std::string(exception_error.what()), "trading_system.log");
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
            } catch (...) {
                log_message("AccountDataThread loop iteration unknown exception", "trading_system.log");
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_account_data_poll_interval_sec));
            }
        }
    } catch (const std::exception& exception_error) {
        log_message("AccountDataThread account_data_loop exception: " + std::string(exception_error.what()), "trading_system.log");
    } catch (...) {
        log_message("AccountDataThread account_data_loop unknown exception", "trading_system.log");
    }
}

// ========================================================================
// ACCOUNT DATA PROCESSING
// ========================================================================

void AccountDataThread::fetch_and_update_account_data() {
    AccountDataCoordinator::AccountDataSnapshotState snapshot_state{
        account_snapshot,
        state_mtx,
        data_cv,
        has_account
    };
    
    account_data_coordinator.update_shared_account_snapshot(snapshot_state);
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

bool AccountDataThread::is_fetch_allowed() const {
    try {
        if (!allow_fetch_ptr) {
            log_message("AccountDataThread allow_fetch_ptr is null", "trading_system.log");
            return false;
        }
        bool allowed = allow_fetch_ptr->load();
        if (!allowed) {
            log_message("AccountDataThread fetch not allowed by gate", "trading_system.log");
        }
        return allowed;
    } catch (const std::exception& exception_error) {
        log_message("AccountDataThread is_fetch_allowed exception: " + std::string(exception_error.what()), "trading_system.log");
        return false;
    } catch (...) {
        log_message("AccountDataThread is_fetch_allowed unknown exception", "trading_system.log");
        return false;
    }
}
