/**
 * Market gate control thread.
 * Manages when market data fetching is allowed based on market hours and connectivity.
 */
#include "market_gate_thread.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/startup_logs.hpp"
#include "core/threads/thread_logic/platform/thread_control.hpp"
#include "core/utils/connectivity_manager.hpp"
#include <chrono>

// Using declarations for cleaner code
using namespace AlpacaTrader::Threads;
using namespace AlpacaTrader::Logging;

// ========================================================================
// THREAD LIFECYCLE MANAGEMENT
// ========================================================================

void MarketGateThread::operator()() {
    set_log_thread_tag("GATE  ");
    
    try {
        // Wait for main thread to complete priority setup
        std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_sequence_delay_milliseconds));
        
        // Start the market gate monitoring loop
        execute_market_gate_monitoring_loop();
    } catch (const std::exception& exception) {
        log_message("MarketGateThread exception: " + std::string(exception.what()), logging.log_file);
    } catch (...) {
        log_message("MarketGateThread unknown exception", logging.log_file);
    }
}

void MarketGateThread::execute_market_gate_monitoring_loop() {
    try {
        bool last_within_trading_hours = api_manager.is_within_trading_hours();
        allow_fetch.store(last_within_trading_hours);
        
        ConnectivityManager::ConnectionStatus last_connectivity_status = connectivity_manager.get_status();
        
        while (running.load()) {
            try {
                check_and_update_fetch_window(last_within_trading_hours);
                
                check_and_report_connectivity_status(last_connectivity_status);
                
                // Increment iteration counter for monitoring
                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            } catch (const std::exception& exception) {
                log_message("MarketGateThread loop iteration exception: " + std::string(exception.what()), logging.log_file);
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            } catch (...) {
                log_message("MarketGateThread loop iteration unknown exception", logging.log_file);
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            }
        }
        
        log_message("MarketGateThread loop exited", "trading_system.log");
    } catch (const std::exception& exception) {
        log_message("MarketGateThread market_gate_loop exception: " + std::string(exception.what()), logging.log_file);
    } catch (...) {
        log_message("MarketGateThread market_gate_loop unknown exception", logging.log_file);
    }
}

// ========================================================================
// MARKET GATE PROCESSING
// ========================================================================

void MarketGateThread::check_and_update_fetch_window(bool& last_within_trading_hours) {
    bool currently_within_trading_hours = api_manager.is_within_trading_hours();
    if (currently_within_trading_hours != last_within_trading_hours) {
        allow_fetch.store(currently_within_trading_hours);
        log_message(std::string("Market fetch gate ") + (currently_within_trading_hours ? "ENABLED" : "DISABLED") +
                    " (pre/post window applied)", logging.log_file);
        last_within_trading_hours = currently_within_trading_hours;
    }
}

void MarketGateThread::check_and_report_connectivity_status(ConnectivityManager::ConnectionStatus& last_connectivity_status) {
    ConnectivityManager::ConnectionStatus current_connectivity_status = connectivity_manager.get_status();
    if (current_connectivity_status != last_connectivity_status) {
        std::string status_message = "Connectivity status changed: " + connectivity_manager.get_status_string();
        auto connectivity_state = connectivity_manager.get_state();
        if (current_connectivity_status == ConnectivityManager::ConnectionStatus::DISCONNECTED) {
            status_message += " (retry in " + std::to_string(connectivity_manager.get_seconds_until_retry()) + "s)";
        } else if (current_connectivity_status == ConnectivityManager::ConnectionStatus::DEGRADED) {
            status_message += " (" + std::to_string(connectivity_state.consecutive_failures) + " failures)";
        }
        log_message(status_message, logging.log_file);
        last_connectivity_status = current_connectivity_status;
    }
}
