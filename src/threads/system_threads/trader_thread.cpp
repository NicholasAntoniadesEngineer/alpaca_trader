/**
 * Trader decision thread.
 * Executes the main trading logic and decision making through TradingCoordinator.
 */
#include "trader_thread.hpp"
#include "logging/logger/async_logger.hpp"
#include "logging/logs/thread_logs.hpp"
#include "logging/logs/trading_logs.hpp"
#include <chrono>
#include <thread>
#include <iostream>
#include <cstdlib>

// Using declarations for cleaner code
using namespace AlpacaTrader::Threads;
using namespace AlpacaTrader::Logging;

// ========================================================================
// THREAD LIFECYCLE MANAGEMENT
// ========================================================================

void TraderThread::operator()() {

    try {
        
        set_log_thread_tag("DECIDE");
        
        
        // Wait for main thread to complete priority setup and other threads to initialize
        std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_sequence_delay_milliseconds + 2000)); // Extra 2 seconds


        // Start the trading decision loop
        execute_trading_decision_loop();
        
    } catch (const std::exception& exception) {
        try {
            ThreadLogs::log_thread_exception("TraderThread", std::string(exception.what()));
            log_message("TraderThread exception: " + std::string(exception.what()), "trading_system.log");
        } catch (...) {
            std::cerr << "CRITICAL: TraderThread exception: " << exception.what() << std::endl;
        }
        
        std::cerr << "CRITICAL: TraderThread failed - terminating thread" << std::endl;
        throw std::runtime_error("TraderThread exception: " + std::string(exception.what()));
    } catch (...) {
        try {
            ThreadLogs::log_thread_unknown_exception("TraderThread");
            log_message("TraderThread unknown exception", "trading_system.log");
        } catch (...) {
            std::cerr << "CRITICAL: TraderThread unknown exception" << std::endl;
        }
        
        std::cerr << "CRITICAL: TraderThread unknown error - terminating thread" << std::endl;
        throw std::runtime_error("TraderThread unknown exception");
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
                trading_coordinator.process_trading_cycle_iteration(
                    market_snapshot,
                    account_snapshot,
                    state_mtx,
                    data_cv,
                    has_market,
                    has_account,
                    running,
                    market_data_timestamp,
                    market_data_fresh,
                    last_order_timestamp,
                    allow_fetch_ptr,
                    initial_equity,
                    loop_counter
                );


                // Increment iteration counter
                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }


                // Wait before next cycle
                trading_coordinator.countdown_to_next_cycle(running, timing.thread_trader_poll_interval_sec, timing.countdown_display_refresh_interval_seconds);


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
