/**
 * Market data collection and processing thread.
 * Fetches real-time market data for trading decisions.
 */
#include "threads/market_data_thread.hpp"
#include "logging/async_logger.hpp"
#include "logging/startup_logger.hpp"
#include "threads/platform/thread_control.hpp"
#include "core/market_processing.hpp"
#include "utils/connectivity_manager.hpp"
#include "configs/component_configs.hpp"
#include <atomic>
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
    
    // Wait for main thread to complete priority setup
    std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_delay_ms));
    
    // Start the market data collection loop
    market_data_loop();
}

void AlpacaTrader::Threads::MarketDataThread::market_data_loop() {
    while (running.load()) {
        if ((allow_fetch_ptr && !allow_fetch_ptr->load()) || !client.is_within_fetch_window()) {
            std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
            continue;
        }
        
        fetch_and_process_market_data();
        
        // Increment iteration counter for monitoring
        if (iteration_counter) {
            iteration_counter->fetch_add(1);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
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


