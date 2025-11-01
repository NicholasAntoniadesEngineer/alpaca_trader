/**
 * Market gate control thread.
 * Manages when market data fetching is allowed based on market hours and connectivity.
 */
#include "market_gate_thread.hpp"
#include "logging/logs/market_gate_logs.hpp"
#include "threads/thread_logic/platform/thread_control.hpp"
#include "utils/connectivity_manager.hpp"
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
        MarketGateLogs::log_thread_starting();
        
        // Start the market gate monitoring loop
        execute_market_gate_monitoring_loop();
    } catch (const std::exception& exception) {
        MarketGateLogs::log_exception(exception.what());
    } catch (...) {
        MarketGateLogs::log_unknown_exception();
    }
}

void MarketGateThread::execute_market_gate_monitoring_loop() {
    try {
        MarketGateLogs::log_before_initial_hours_check();
        bool last_within_trading_hours = false;
        try {
            last_within_trading_hours = api_manager.is_within_trading_hours(trading_symbol);
            connectivity_manager.report_success();
        } catch (const std::exception& trading_hours_check_exception_error) {
            connectivity_manager.report_failure(trading_hours_check_exception_error.what());
            last_within_trading_hours = false;
        } catch (...) {
            connectivity_manager.report_failure("Unknown error in is_within_trading_hours");
            last_within_trading_hours = false;
        }
        MarketGateLogs::log_initial_hours_state(last_within_trading_hours);
        allow_fetch.store(last_within_trading_hours);
        
        ConnectivityManager::ConnectionStatus last_connectivity_status = connectivity_manager.get_status();
        
        while (running.load()) {
            try {
                MarketGateLogs::log_before_update_fetch_window();
                check_and_update_fetch_window(last_within_trading_hours);
                MarketGateLogs::log_after_update_fetch_window();
                
                MarketGateLogs::log_before_connectivity_status();
                check_and_report_connectivity_status(last_connectivity_status);
                MarketGateLogs::log_after_connectivity_status();
                
                // Increment iteration counter for monitoring
                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            } catch (const std::exception& exception) {
                MarketGateLogs::log_loop_exception(exception.what());
                connectivity_manager.report_failure(exception.what());
                allow_fetch.store(false);
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            } catch (...) {
                MarketGateLogs::log_loop_unknown_exception();
                connectivity_manager.report_failure("Unknown gate loop error");
                allow_fetch.store(false);
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            }
        }
        
        MarketGateLogs::log_loop_exited();
    } catch (const std::exception& exception) {
        MarketGateLogs::log_market_gate_loop_exception(exception.what());
    } catch (...) {
        MarketGateLogs::log_market_gate_loop_unknown_exception();
    }
}

// ========================================================================
// MARKET GATE PROCESSING
// ========================================================================

void MarketGateThread::check_and_update_fetch_window(bool& last_within_trading_hours) {
    MarketGateLogs::log_before_api_check();
    bool currently_within_trading_hours = false;
    try {
        currently_within_trading_hours = api_manager.is_within_trading_hours(trading_symbol);
        connectivity_manager.report_success();
    } catch (const std::exception& trading_hours_status_exception_error) {
        connectivity_manager.report_failure(trading_hours_status_exception_error.what());
        currently_within_trading_hours = false;
    } catch (...) {
        connectivity_manager.report_failure("Unknown error in trading hours check");
        currently_within_trading_hours = false;
    }
    MarketGateLogs::log_current_hours_state(currently_within_trading_hours);
    if (currently_within_trading_hours != last_within_trading_hours) {
        allow_fetch.store(currently_within_trading_hours);
        MarketGateLogs::log_fetch_gate_state(currently_within_trading_hours);
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
        MarketGateLogs::log_connectivity_status_changed(status_message);
        last_connectivity_status = current_connectivity_status;
    }
}
