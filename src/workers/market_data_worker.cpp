// MarketDataTask.cpp
#include "workers/market_data_worker.hpp"
#include "utils/async_logger.hpp"
#include "utils/indicators.hpp"
#include <atomic>
#include <chrono>

void MarketDataTask::operator()() {
    set_log_thread_tag("MARKET");
    while (running.load()) {
        if ((allow_fetch_ptr && !allow_fetch_ptr->load()) || !client.is_within_fetch_window()) {
            std::this_thread::sleep_for(std::chrono::seconds(timing.sleep_interval_sec));
            continue;
        }
        int num_bars = strategy.atr_period + timing.bar_buffer;
        BarRequest br{target.symbol, num_bars};
        auto bars = client.get_recent_bars(br);
        if (static_cast<int>(bars.size()) >= strategy.atr_period + 2) {
            std::vector<double> highs, lows, closes; std::vector<long long> vols;
            highs.reserve(bars.size()); lows.reserve(bars.size()); closes.reserve(bars.size()); vols.reserve(bars.size());
            for (const auto& b : bars) { highs.push_back(b.h); lows.push_back(b.l); closes.push_back(b.c); vols.push_back(b.v); }
            MarketSnapshot temp;
            temp.atr = calculate_atr(highs, lows, closes, strategy.atr_period);
            temp.avg_atr = calculate_atr(highs, lows, closes, strategy.atr_period * strategy.avg_atr_multiplier);
            temp.avg_vol = calculate_avg_volume(vols, strategy.atr_period);
            temp.curr = bars.back();
            temp.prev = bars[bars.size()-2];
            if (temp.atr != 0.0) {
                std::lock_guard<std::mutex> lock(state_mtx);
                market_snapshot = temp;
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
    while (running.load()) {
        bool within = client.is_within_fetch_window();
        bool prev = allow_fetch.load();
        allow_fetch.store(within);
        if (within != prev) {
            log_message(std::string("Market fetch gate ") + (within ? "ENABLED" : "DISABLED") +
                        " (pre/post window applied)", logging.log_file);
        }
        log_message(std::string("Market fetch gate CHECK: ") + (within ? "ENABLED" : "DISABLED") +
                    " | interval=" + std::to_string(timing.market_open_check_sec) + "s" +
                    " | buffers=" + std::to_string(timing.pre_open_buffer_min) + "m/" +
                    std::to_string(timing.post_close_buffer_min) + "m",
                    logging.log_file);
        std::this_thread::sleep_for(std::chrono::seconds(timing.market_open_check_sec));
    }
}

void MarketGateTask::operator()() {
    run_market_gate(running, allow_fetch, timing, logging, client);
}
