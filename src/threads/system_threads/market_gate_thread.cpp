/**
 * Market gate control thread.
 * Manages when market data fetching is allowed based on market hours and connectivity.
 */
#include "market_gate_thread.hpp"
#include "logging/logs/market_gate_logs.hpp"
#include "threads/thread_logic/platform/thread_control.hpp"
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
        market_gate_coordinator.check_and_update_fetch_window(trading_symbol, allow_fetch, last_within_trading_hours);
        MarketGateLogs::log_initial_hours_state(last_within_trading_hours);
        
        ConnectivityManager::ConnectionStatus last_connectivity_status = market_gate_coordinator.get_connectivity_status();
        
        while (running.load()) {
            try {
                MarketGateLogs::log_before_update_fetch_window();
                market_gate_coordinator.check_and_update_fetch_window(trading_symbol, allow_fetch, last_within_trading_hours);
                MarketGateLogs::log_after_update_fetch_window();
                
                MarketGateLogs::log_before_connectivity_status();
                market_gate_coordinator.check_and_report_connectivity_status(last_connectivity_status);
                MarketGateLogs::log_after_connectivity_status();
                
                // Increment iteration counter for monitoring
                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            } catch (const std::exception& exception) {
                MarketGateLogs::log_loop_exception(exception.what());
                allow_fetch.store(false);
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            } catch (...) {
                MarketGateLogs::log_loop_unknown_exception();
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
