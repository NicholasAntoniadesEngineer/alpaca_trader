/**
 * Trader decision thread.
 * Executes the main trading logic and decision making through TradingCoordinator.
 */
#include "trader_thread.hpp"
#include "logging/logger/async_logger.hpp"
#include "logging/logs/startup_logs.hpp"
#include "logging/logs/thread_logs.hpp"
#include "logging/logs/trading_logs.hpp"
#include <chrono>
#include <thread>

// Using declarations for cleaner code
using namespace AlpacaTrader::Threads;
using namespace AlpacaTrader::Logging;

// ========================================================================
// THREAD LIFECYCLE MANAGEMENT
// ========================================================================

void TraderThread::operator()() {
    set_log_thread_tag("DECIDE");

    try {
        // Wait for main thread to complete priority setup and other threads to initialize
        std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_sequence_delay_milliseconds + 2000)); // Extra 2 seconds

        // Start the trading decision loop
        execute_trading_decision_loop();
    } catch (const std::exception& exception) {
        ThreadLogs::log_thread_exception("TraderThread", std::string(exception.what()));
        log_message("TraderThread exception: " + std::string(exception.what()), "trading_system.log");
    } catch (...) {
        ThreadLogs::log_thread_unknown_exception("TraderThread");
        log_message("TraderThread unknown exception", "trading_system.log");
    }
}

void TraderThread::execute_trading_decision_loop() {
    try {
        while (running.load()) {
            try {
                // Check if we should continue running
                if (!running.load()) {
                    break;
                }

                // Process one trading cycle iteration
                process_trading_cycle_iteration();

                // Increment iteration counter
                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }

                // Wait before next cycle
                countdown_to_next_cycle();

            } catch (const std::exception& exception_error) {
                TradingLogs::log_market_data_result_table("Exception in trading cycle (execute_trading_decision_loop): " + std::string(exception_error.what()), false, 0);
                int recovery_sleep_seconds = 5; // Use config if available
                std::this_thread::sleep_for(std::chrono::seconds(recovery_sleep_seconds));
            } catch (...) {
                TradingLogs::log_market_data_result_table("Unknown exception in trading cycle (execute_trading_decision_loop)", false, 0);
                int recovery_sleep_seconds = 5;
                std::this_thread::sleep_for(std::chrono::seconds(recovery_sleep_seconds));
            }
        }
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_data_result_table("Critical exception in trading loop (execute_trading_decision_loop outer): " + std::string(exception_error.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Critical unknown exception in trading loop (execute_trading_decision_loop outer)", false, 0);
    }
}

// ========================================================================
// TRADING CYCLE PROCESSING
// ========================================================================

void TraderThread::process_trading_cycle_iteration() {
    try {
        // Increment loop counter
        loop_counter.fetch_add(1);

        // Create snapshot state structure
        AlpacaTrader::Core::TradingCoordinator::TradingSnapshotState snapshot_state{
            market_snapshot,
            account_snapshot,
            state_mtx,
            data_cv,
            has_market,
            has_account,
            running
        };

        // Create market data sync state structure
        AlpacaTrader::Core::MarketDataSyncState market_data_sync_state(
            &state_mtx,
            &data_cv,
            &market_snapshot,
            &account_snapshot,
            &has_market,
            &has_account,
            &running,
            allow_fetch_ptr ? allow_fetch_ptr : &running,  // Use allow_fetch if available, otherwise running
            &market_data_timestamp,
            &market_data_fresh,
            &last_order_timestamp
        );

        // Execute trading cycle iteration through coordinator
        trading_coordinator.execute_trading_cycle_iteration(snapshot_state, market_data_sync_state, initial_equity, loop_counter.load());

    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_data_result_table("Exception in process_trading_cycle_iteration: " + std::string(exception_error.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown exception in process_trading_cycle_iteration", false, 0);
    }
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

void TraderThread::countdown_to_next_cycle() {
    int sleep_secs = timing.thread_trader_poll_interval_sec;
    int countdown_refresh_interval = timing.countdown_display_refresh_interval_seconds;

    // If countdown refresh interval is 0 or greater than sleep_secs, just sleep once
    if (countdown_refresh_interval <= 0 || countdown_refresh_interval >= sleep_secs) {
        if (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(sleep_secs));
        }
        return;
    }

    // Calculate how many countdown updates we should show
    int num_updates = sleep_secs / countdown_refresh_interval;
    int remaining_secs = sleep_secs;

    for (int update_index = 0; update_index < num_updates && running.load(); ++update_index) {
        int display_secs = std::min(remaining_secs, countdown_refresh_interval);
        TradingLogs::log_inline_next_loop(display_secs);

        if (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(countdown_refresh_interval));
            remaining_secs -= countdown_refresh_interval;
        }
    }

    // Sleep any remaining time without countdown display
    if (remaining_secs > 0 && running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(remaining_secs));
    }

    TradingLogs::end_inline_status();
}
