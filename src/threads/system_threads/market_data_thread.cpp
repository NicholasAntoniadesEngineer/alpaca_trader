/**
 * Market data collection and processing thread.
 * Fetches real-time market data for trading decisions.
 */
#include "market_data_thread.hpp"
#include "logging/async_logger.hpp"
#include "logging/startup_logs.hpp"
#include "../thread_logic/platform/thread_control.hpp"
#include "core/trader/market_processing.hpp"
#include "utils/connectivity_manager.hpp"
#include "configs/component_configs.hpp"
#include <chrono>

// Using declarations for cleaner code
using AlpacaTrader::Threads::MarketDataThread;
using AlpacaTrader::Logging::set_log_thread_tag;
using AlpacaTrader::Logging::log_message;
using AlpacaTrader::Core::BarRequest;
using AlpacaTrader::Core::ProcessedData;
using AlpacaTrader::API::AlpacaClient;

void AlpacaTrader::Threads::MarketDataThread::operator()() {
    set_log_thread_tag("MARKET");
    
    try {
        // Wait for main thread to complete priority setup
        std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_delay_ms));
        
        // Start the market data collection loop
        market_data_loop();
    } catch (const std::exception& e) {
        log_message("MarketDataThread exception: " + std::string(e.what()), "trading_system.log");
    } catch (...) {
        log_message("MarketDataThread unknown exception", "trading_system.log");
    }
}

void AlpacaTrader::Threads::MarketDataThread::market_data_loop() {
    try {
        while (running.load()) {
            try {
                if (!allow_fetch_ptr || !allow_fetch_ptr->load()) {
                    std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
                    continue;
                }

                fetch_and_process_market_data();

                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }

                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
            } catch (const std::exception& e) {
                log_message("MarketDataThread loop iteration exception: " + std::string(e.what()), "trading_system.log");
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
            } catch (...) {
                log_message("MarketDataThread loop iteration unknown exception", "trading_system.log");
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
            }
        }
    } catch (const std::exception& e) {
        log_message("MarketDataThread market_data_loop exception: " + std::string(e.what()), "trading_system.log");
    } catch (...) {
        log_message("MarketDataThread market_data_loop unknown exception", "trading_system.log");
    }
}

void AlpacaTrader::Threads::MarketDataThread::fetch_and_process_market_data() {
    int num_bars = strategy.atr_period + timing.bar_buffer;
    BarRequest br{target.symbol, num_bars};
    auto bars = client.get_recent_bars(br);
    
    if (static_cast<int>(bars.size()) >= strategy.atr_period + 2) {
        // Compute indicators using the same implementation as Trader
        TraderConfig minimal_cfg{StrategyConfig{strategy}, RiskConfig{}, TimingConfig{timing}, LoggingConfig{}, TargetConfig{target}};
        ProcessedData computed = AlpacaTrader::Core::MarketProcessing::compute_processed_data(bars, minimal_cfg);

        if (computed.atr != 0.0) {
            update_market_snapshot(computed);
        }
    }
}

void AlpacaTrader::Threads::MarketDataThread::update_market_snapshot(const ProcessedData& computed) {
    std::lock_guard<std::mutex> lock(state_mtx);
    market_snapshot.atr = computed.atr;
    market_snapshot.avg_atr = computed.avg_atr;
    market_snapshot.avg_vol = computed.avg_vol;
    market_snapshot.curr = computed.curr;
    market_snapshot.prev = computed.prev;
    has_market.store(true);
    data_cv.notify_all();
}


