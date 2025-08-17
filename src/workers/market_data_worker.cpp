// MarketDataWorker.cpp
#include "workers/market_data_worker.hpp"
#include "utils/async_logger.hpp"
#include "utils/indicators.hpp"
#include <chrono>

MarketDataWorker::MarketDataWorker(const StrategyConfig& strategyCfg,
                                   const TimingConfig& timingCfg,
                                   const TargetConfig& targetCfg,
                                   AlpacaClient& cli,
                                   std::mutex& mtx,
                                   std::condition_variable& cv,
                                   MarketSnapshot& snapshot,
                                   std::atomic<bool>& has_market_flag,
                                   std::atomic<bool>& running_flag)
    : strategy(strategyCfg), timing(timingCfg), target(targetCfg), client(cli), state_mtx(mtx), data_cv(cv), market_snapshot(snapshot), has_market(has_market_flag), running(running_flag) {}

void MarketDataWorker::start() {
    worker = std::thread([this](){
        set_log_thread_tag("MARKET");
        while (running.load()) {
            // Respect fetch gate and live clock check
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
    });
}

void MarketDataWorker::stop() { running.store(false); data_cv.notify_all(); }
void MarketDataWorker::join() { if (worker.joinable()) worker.join(); }
