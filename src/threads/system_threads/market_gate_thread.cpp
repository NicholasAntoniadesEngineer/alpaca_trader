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
        
        // Start the market gate monitoring loop
        execute_market_gate_monitoring_loop();
    } catch (const std::exception& exception) {
        // Exception caught, thread will exit
    } catch (...) {
        // Unknown exception caught, thread will exit
    }
}

void MarketGateThread::execute_market_gate_monitoring_loop() {
    try {
        bool last_within_trading_hours = false;
        market_gate_coordinator.check_and_update_fetch_window(trading_symbol, allow_fetch, last_within_trading_hours);
        
        ConnectivityManager::ConnectionStatus last_connectivity_status = market_gate_coordinator.get_connectivity_status();
        
        while (running.load()) {
            try {
                market_gate_coordinator.check_and_update_fetch_window(trading_symbol, allow_fetch, last_within_trading_hours);
                
                market_gate_coordinator.check_and_report_connectivity_status(last_connectivity_status);
                
                // Increment iteration counter for monitoring
                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            } catch (const std::exception& exception) {
                allow_fetch.store(false);
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            } catch (...) {
                allow_fetch.store(false);
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_gate_poll_interval_sec));
            }
        }
        
    } catch (const std::exception& exception) {
        // Outer exception caught, loop will exit
    } catch (...) {
        // Unknown exception caught, loop will exit
    }
}
