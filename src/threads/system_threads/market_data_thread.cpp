/**
 * Market data collection and processing thread.
 * 
 * This thread is responsible for:
 * - Fetching historical market data for technical analysis
 * - Computing ATR and other technical indicators
 * - Processing real-time quote data
 * - Logging market data to CSV files
 * - Updating shared market data snapshots
 */
#include "market_data_thread.hpp"
#include "logging/logs/market_data_thread_logs.hpp"
#include "logging/logger/async_logger.hpp"
#include "threads/thread_logic/platform/thread_control.hpp"
#include <chrono>

// Using declarations for cleaner code
using namespace AlpacaTrader::Threads;
using namespace AlpacaTrader::Logging;
using namespace AlpacaTrader::Core;
using namespace AlpacaTrader::API;
using namespace AlpacaTrader::Config;

// ========================================================================
// THREAD LIFECYCLE MANAGEMENT
// ========================================================================

void MarketDataThread::operator()() {
    set_log_thread_tag("MARKET");
    
    try {
        SystemConfig config;
        config.strategy = strategy;
        config.timing = timing;
        MarketDataThreadLogs::log_thread_startup(config);
        
        // Wait for main thread to complete priority setup
        std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_sequence_delay_milliseconds));
        
        // Start the market data collection loop
        execute_market_data_collection_loop();
    } catch (const std::exception& exception_error) {
        MarketDataThreadLogs::log_thread_exception(exception_error.what());
    } catch (...) {
        MarketDataThreadLogs::log_thread_exception("Unknown exception");
    }
}

void MarketDataThread::execute_market_data_collection_loop() {
    try {
        while (running.load()) {
            try {
                if (!MarketDataThreadLogs::is_fetch_allowed(allow_fetch_ptr)) {
                    std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
                    continue;
                }

                MarketDataCoordinator::MarketDataSnapshotState snapshot_state{
                    market_snapshot,
                    state_mtx,
                    data_cv,
                    has_market,
                    market_data_timestamp,
                    market_data_fresh
                };
                
                market_data_coordinator.process_market_data_iteration(strategy.symbol, snapshot_state, last_bar_log_time, previous_bar);

                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }

                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
            } catch (const std::exception& exception_error) {
                MarketDataThreadLogs::log_thread_loop_exception(exception_error.what());
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
            } catch (...) {
                MarketDataThreadLogs::log_thread_loop_exception("Unknown exception");
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
            }
        }
    } catch (const std::exception& exception_error) {
        MarketDataThreadLogs::log_thread_exception("Loop exception: " + std::string(exception_error.what()));
    } catch (...) {
        MarketDataThreadLogs::log_thread_exception("Loop unknown exception");
    }
}


