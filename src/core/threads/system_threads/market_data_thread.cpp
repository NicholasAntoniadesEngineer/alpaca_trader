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
#include "core/logging/logs/market_data_thread_logs.hpp"
#include "core/logging/logger/async_logger.hpp"
#include "core/logging/logs/startup_logs.hpp"
#include "core/logging/logger/logging_macros.hpp"
#include "core/trader/data/market_data_validator.hpp"
#include "core/threads/thread_logic/platform/thread_control.hpp"
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
        MarketDataThreadLogs::log_thread_loop_exception("Entered execute_market_data_collection_loop");
        while (running.load()) {
            try {
                MarketDataThreadLogs::log_thread_loop_exception("Before fetch gate check");
                if (!MarketDataThreadLogs::is_fetch_allowed(allow_fetch_ptr)) {
                    MarketDataThreadLogs::log_thread_loop_exception("Fetch not allowed - sleeping");
                    std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
                    continue;
                }
                MarketDataThreadLogs::log_thread_loop_exception("After fetch gate check - starting iteration");

                process_market_data_iteration();

                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }

                MarketDataThreadLogs::log_thread_loop_exception("Iteration complete - sleeping");
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

// ========================================================================
// MARKET DATA PROCESSING
// ========================================================================

void MarketDataThread::process_market_data_iteration() {
    try {
        LOG_THREAD_SECTION_HEADER("MARKET DATA FETCH - " + strategy.symbol);
        
        SystemConfig thread_config;
        thread_config.strategy = strategy;
        thread_config.timing = timing;
        
        std::vector<Bar> historical_bars_for_logging;
        ProcessedData computed_data = market_data_coordinator.fetch_and_process_market_data(strategy.symbol, historical_bars_for_logging);
        
        if (computed_data.atr == 0.0) {
            MarketDataThreadLogs::log_zero_atr_warning(strategy.symbol);
            return;
        }
        
        MarketDataCoordinator::MarketDataSnapshotState snapshot_state{
            market_snapshot,
            state_mtx,
            data_cv,
            has_market,
            market_data_timestamp,
            market_data_fresh
        };
        
        market_data_coordinator.update_shared_market_snapshot(computed_data, snapshot_state);
        
        MarketDataValidator validator_instance(thread_config);
        API::ApiManager& api_manager_ref = market_data_coordinator.get_api_manager_reference();
        MarketDataThreadLogs::process_csv_logging_if_needed(computed_data, historical_bars_for_logging, validator_instance, strategy.symbol, timing, api_manager_ref, last_bar_log_time, previous_bar);
        
        LOG_THREAD_SECTION_FOOTER();
    } catch (const std::exception& exception_error) {
        MarketDataThreadLogs::log_thread_loop_exception("Error in process_market_data_iteration: " + std::string(exception_error.what()));
    } catch (...) {
        MarketDataThreadLogs::log_thread_loop_exception("Unknown error in process_market_data_iteration");
    }
}

