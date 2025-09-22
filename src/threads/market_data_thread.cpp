/**
 * Market data collection and processing thread.
 * Fetches real-time market data for trading decisions.
 */
#include "market_data_thread.hpp"
#include "../logging/async_logger.hpp"
#include "../logging/startup_logger.hpp"
#include "platform/thread_control.hpp"
#include "../core/market_processing.hpp"
#include "../utils/connectivity_manager.hpp"
#include <atomic>
#include <chrono>

void MarketDataThread::operator()() {
    set_log_thread_tag("MARKET");
    
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
        
        // Increment iteration counter for monitoring
        if (iteration_counter) {
            iteration_counter->fetch_add(1);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(timing.sleep_interval_sec));
    }
}

void run_market_gate(std::atomic<bool>& running,
                     std::atomic<bool>& allow_fetch,
                     const TimingConfig& timing,
                     const LoggingConfig& logging,
                     AlpacaClient& client,
                     std::atomic<unsigned long>* iteration_counter) {
    set_log_thread_tag("GATE  ");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    bool last_within = client.is_within_fetch_window();
    allow_fetch.store(last_within);
    
    auto& connectivity = ConnectivityManager::instance();
    ConnectivityManager::ConnectionStatus last_connectivity_status = connectivity.get_status();
    
    while (running.load()) {
        bool within = client.is_within_fetch_window();
        if (within != last_within) {
            allow_fetch.store(within);
            log_message(std::string("Market fetch gate ") + (within ? "ENABLED" : "DISABLED") +
                        " (pre/post window applied)", logging.log_file);
            last_within = within;
        }
        
        // Monitor and report connectivity status changes
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
        
        // Increment iteration counter for monitoring
        if (iteration_counter) {
            iteration_counter->fetch_add(1);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(timing.market_open_check_sec));
    }
}

void MarketGateThread::operator()() {
    run_market_gate(running, allow_fetch, timing, logging, client, iteration_counter);
}
