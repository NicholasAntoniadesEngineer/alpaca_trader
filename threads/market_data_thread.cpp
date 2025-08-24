// MarketDataThread.cpp
#include "market_data_thread.hpp"
#include "../utils/async_logger.hpp"
#include "platform/thread_control.hpp"
#include "../core/market_processing.hpp"
#include <atomic>
#include <chrono>

void MarketDataThread::operator()() {
    set_log_thread_tag("MARKET");
    log_message("   |  • Market data thread started: " + ThreadSystem::Platform::ThreadControl::get_thread_info(), "");
    
    // Wait for main thread to complete priority setup
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    while (running.load()) {
        if ((allow_fetch_ptr && !allow_fetch_ptr->load()) || !client.is_within_fetch_window()) {
            std::this_thread::sleep_for(std::chrono::seconds(timing.sleep_interval_sec));
            continue;
        }
        int num_bars = strategy.atr_period + timing.bar_buffer;
        BarRequest br{target.symbol, num_bars};
        auto bars = client.get_recent_bars(br);
        if (static_cast<int>(bars.size()) >= strategy.atr_period + 2) {
            // Compute indicators using the same implementation as Trader
            TraderConfig minimal_cfg{StrategyConfig{strategy}, RiskConfig{}, TimingConfig{timing}, FlagsConfig{}, UXConfig{}, LoggingConfig{}, TargetConfig{target}};
            ProcessedData computed = MarketProcessing::compute_processed_data(bars, minimal_cfg);

            if (computed.atr != 0.0) {
                std::lock_guard<std::mutex> lock(state_mtx);
                market_snapshot.atr = computed.atr;
                market_snapshot.avg_atr = computed.avg_atr;
                market_snapshot.avg_vol = computed.avg_vol;
                market_snapshot.curr = computed.curr;
                market_snapshot.prev = computed.prev;
                has_market.store(true);
                data_cv.notify_all();
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(timing.sleep_interval_sec));
    }
}

void run_market_gate(std::atomic<bool>& running,
                     std::atomic<bool>& allow_fetch,
                     const TimingConfig& timing,
                     const LoggingConfig& logging,
                     AlpacaClient& client) {
    set_log_thread_tag("GATE  ");
    log_message("   |  • Market gate thread started: " + ThreadSystem::Platform::ThreadControl::get_thread_info(), logging.log_file);
    
    // Wait for main thread to complete priority setup
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    bool last_within = client.is_within_fetch_window();
    allow_fetch.store(last_within);
    while (running.load()) {
        bool within = client.is_within_fetch_window();
        if (within != last_within) {
            allow_fetch.store(within);
            log_message(std::string("Market fetch gate ") + (within ? "ENABLED" : "DISABLED") +
                        " (pre/post window applied)", logging.log_file);
            last_within = within;
        }
        std::this_thread::sleep_for(std::chrono::seconds(timing.market_open_check_sec));
    }
}

void MarketGateThread::operator()() {
    run_market_gate(running, allow_fetch, timing, logging, client);
}
