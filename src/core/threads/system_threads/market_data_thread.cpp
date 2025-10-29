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
#include "core/logging/logger/csv_bars_logger.hpp"
#include "core/logging/logs/startup_logs.hpp"
#include "core/logging/logger/logging_macros.hpp"
#include "core/threads/thread_logic/platform/thread_control.hpp"
#include "core/trader/data/market_processing.hpp"
#include "core/trader/data/bars_data_manager.hpp"
#include "core/trader/data/market_data_validator.hpp"
#include "core/utils/connectivity_manager.hpp"
#include "core/utils/time_utils.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>

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
    } catch (const std::exception& exception) {
        MarketDataThreadLogs::log_thread_exception(exception.what());
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

                process_market_data_iteration();

                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }

                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
            } catch (const std::exception& exception) {
                MarketDataThreadLogs::log_thread_loop_exception(exception.what());
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
            } catch (...) {
                MarketDataThreadLogs::log_thread_loop_exception("Unknown exception");
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
            }
        }
    } catch (const std::exception& exception) {
        MarketDataThreadLogs::log_thread_exception("Loop exception: " + std::string(exception.what()));
    } catch (...) {
        MarketDataThreadLogs::log_thread_exception("Loop unknown exception");
    }
}

// ========================================================================
// MARKET DATA PROCESSING
// ========================================================================

void MarketDataThread::process_market_data_iteration() {
    LOG_THREAD_SECTION_HEADER("MARKET DATA FETCH - " + strategy.symbol);
    
    // Create managers for data processing
    SystemConfig config;
    config.strategy = strategy;
    config.timing = timing;
    
    BarsDataManager bars_manager(config, api_manager);
    MarketDataValidator validator(config);
    
    // Fetch historical bars for technical analysis
    MarketDataFetchRequest fetch_request(strategy.symbol, 
                                       strategy.bars_to_fetch_for_calculations + timing.historical_data_buffer_size,
                                       strategy.atr_calculation_bars);
    
    auto historical_bars = bars_manager.fetch_historical_market_data(fetch_request);
    
    if (historical_bars.empty()) {
        LOG_THREAD_CONTENT("No bars received from API");
        LOG_THREAD_SECTION_FOOTER();
        return;
    }
    
    // Validate we have sufficient data for calculations
    if (!bars_manager.has_sufficient_bars_for_calculations(historical_bars, fetch_request.atr_calculation_bars)) {
        LOG_THREAD_CONTENT("Insufficient bars for ATR calculation");
        LOG_THREAD_SECTION_FOOTER();
        return;
    }
    
    // Compute technical indicators using MarketProcessing
    SystemConfig minimal_configuration;
    minimal_configuration.strategy = strategy;
    minimal_configuration.timing = timing;
    
    ProcessedData computed_data = MarketProcessing::compute_processed_data(historical_bars, minimal_configuration);
    
    if (computed_data.atr == 0.0) {
        MarketDataThreadLogs::log_zero_atr_warning(strategy.symbol);
        LOG_THREAD_SECTION_FOOTER();
        return;
    }
    
    // Update shared market data snapshot
    update_market_data_snapshot(computed_data);
    
    // Process CSV logging if needed
    MarketDataThreadLogs::process_csv_logging_if_needed(computed_data, historical_bars, validator, strategy.symbol, timing, api_manager, last_bar_log_time, previous_bar);
    
    LOG_THREAD_SECTION_FOOTER();
}

void MarketDataThread::update_market_data_snapshot(const ProcessedData& computed_data) {
    MarketDataThreadLogs::log_market_snapshot_update(strategy.symbol);
    
    std::lock_guard<std::mutex> state_lock(state_mtx);

    market_snapshot.atr = computed_data.atr;
    market_snapshot.avg_atr = computed_data.avg_atr;
    market_snapshot.avg_vol = computed_data.avg_vol;
    market_snapshot.curr = computed_data.curr;
    market_snapshot.prev = computed_data.prev;
    has_market.store(true);

    // Update data freshness timestamp
    auto current_time = std::chrono::steady_clock::now();
    market_data_timestamp.store(current_time);
    market_data_fresh.store(true);
    
    data_cv.notify_all();
}
