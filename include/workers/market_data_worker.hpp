// market_data_worker.hpp
#ifndef MARKET_DATA_WORKER_HPP
#define MARKET_DATA_WORKER_HPP

#include "../configs/strategy_config.hpp"
#include "../configs/timing_config.hpp"
#include "../configs/target_config.hpp"
#include "../api/alpaca_client.hpp"
#include "../data/data_structures.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

class MarketDataWorker {
private:
    const StrategyConfig& strategy;
    const TimingConfig& timing;
    const TargetConfig& target;
    AlpacaClient& client;
    std::mutex& state_mtx;
    std::condition_variable& data_cv;
    MarketSnapshot& market_snapshot;
    std::atomic<bool>& has_market;
    std::atomic<bool>& running;
    std::atomic<bool>* allow_fetch_ptr {nullptr};
    std::thread worker;

public:
    MarketDataWorker(const StrategyConfig& strategyCfg,
                     const TimingConfig& timingCfg,
                     const TargetConfig& targetCfg,
                     AlpacaClient& cli,
                     std::mutex& mtx,
                     std::condition_variable& cv,
                     MarketSnapshot& snapshot,
                     std::atomic<bool>& has_market_flag,
                     std::atomic<bool>& running_flag);

    void start();
    void stop();
    void join();

    // External gate: when set, worker will fetch; otherwise it sleeps
    void set_allow_fetch_flag(std::atomic<bool>& allow_flag) { allow_fetch_ptr = &allow_flag; }
};

#endif // MARKET_DATA_WORKER_HPP
