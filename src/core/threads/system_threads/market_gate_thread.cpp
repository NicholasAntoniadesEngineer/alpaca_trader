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
using AlpacaTrader::Threads::MarketGateThread;
using AlpacaTrader::Logging::set_log_thread_tag;
using AlpacaTrader::Logging::log_message;

void AlpacaTrader::Threads::MarketGateThread::operator()() {
    set_log_thread_tag("GATE  ");
    
    try {
        // Wait for main thread to complete priority setup
        std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_sequence_delay_milliseconds));
        
        // Start the market gate monitoring loop
        market_gate_loop();
    } catch (const std::exception& e) {
        log_message("MarketGateThread exception: " + std::string(e.what()), logging.log_file);
    } catch (...) {
        log_message("MarketGateThread unknown exception", logging.log_file);
    }
}

void AlpacaTrader::Threads::MarketGateThread::market_gate_loop() {
    try {
        bool last_within = api_manager.is_within_trading_hours();
        allow_fetch.store(last_within);
        
        auto& connectivity = ConnectivityManager::instance();
        ConnectivityManager::ConnectionStatus last_connectivity_status = connectivity.get_status();
        
        while (running.load()) {
            try {
                check_and_update_fetch_window(last_within);
                
                check_and_report_connectivity_status(connectivity, last_connectivity_status);
                
                // Increment iteration counter for monitoring
                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            } catch (const std::exception& e) {
                log_message("MarketGateThread loop iteration exception: " + std::string(e.what()), logging.log_file);
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            } catch (...) {
                log_message("MarketGateThread loop iteration unknown exception", logging.log_file);
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            }
        }
        
        log_message("MarketGateThread loop exited", "trading_system.log");
    } catch (const std::exception& e) {
        log_message("MarketGateThread market_gate_loop exception: " + std::string(e.what()), logging.log_file);
    } catch (...) {
        log_message("MarketGateThread market_gate_loop unknown exception", logging.log_file);
    }
}

void AlpacaTrader::Threads::MarketGateThread::check_and_update_fetch_window(bool& last_within) {
    bool within = api_manager.is_within_trading_hours();
    if (within != last_within) {
        allow_fetch.store(within);
        log_message(std::string("Market fetch gate ") + (within ? "ENABLED" : "DISABLED") +
                    " (pre/post window applied)", logging.log_file);
        last_within = within;
    }
}

void AlpacaTrader::Threads::MarketGateThread::check_and_report_connectivity_status(ConnectivityManager& connectivity,
                     ConnectivityManager::ConnectionStatus& last_connectivity_status) {
    ConnectivityManager::ConnectionStatus current_status = connectivity.get_status();
    if (current_status != last_connectivity_status) {
        std::string status_msg = "Connectivity status changed: " + connectivity.get_status_string();
        auto state = connectivity.get_state();
        if (current_status == ConnectivityManager::ConnectionStatus::DISCONNECTED) {
            status_msg += " (retry in " + std::to_string(connectivity.get_seconds_until_retry()) + "s)";
        } else if (current_status == ConnectivityManager::ConnectionStatus::DEGRADED) {
            status_msg += " (" + std::to_string(state.consecutive_failures) + " failures)";
        }
        log_message(status_msg, logging.log_file);
        last_connectivity_status = current_status;
    }
}
