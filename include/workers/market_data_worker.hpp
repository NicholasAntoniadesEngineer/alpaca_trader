// market_data_worker.hpp
#ifndef MARKET_DATA_WORKER_HPP
#define MARKET_DATA_WORKER_HPP

#include "../configs/strategy_config.hpp"
#include "../configs/timing_config.hpp"
#include "../configs/target_config.hpp"
#include "../api/alpaca_client.hpp"
#include "../configs/component_configs.hpp"
#include "../data/data_structures.hpp"
#include <atomic>
#include <mutex>
#include <condition_variable>

// Task object to be run on a std::thread from main
struct MarketDataTask {
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

    MarketDataTask(const MarketDataTaskConfig& cfg,
                   AlpacaClient& cli,
                   std::mutex& mtx,
                   std::condition_variable& cv,
                   MarketSnapshot& snapshot,
                   std::atomic<bool>& has_market_flag,
                   std::atomic<bool>& running_flag)
        : strategy(cfg.strategy), timing(cfg.timing), target(cfg.target), client(cli),
          state_mtx(mtx), data_cv(cv), market_snapshot(snapshot), has_market(has_market_flag),
          running(running_flag) {}

    // External gate: when set, task will fetch; otherwise it sleeps
    void set_allow_fetch_flag(std::atomic<bool>& allow_flag) { allow_fetch_ptr = &allow_flag; }

    // Thread entrypoint
    void operator()();
};

// Market gate task (loop deciding allow_fetch) – receives only needed state
void run_market_gate(std::atomic<bool>& running,
                     std::atomic<bool>& allow_fetch,
                     const TimingConfig& timing,
                     const LoggingConfig& logging,
                     AlpacaClient& client);

// Task object to run the market gate loop on a std::thread
struct MarketGateTask {
    const TimingConfig& timing;
    const LoggingConfig& logging;
    std::atomic<bool>& allow_fetch;
    std::atomic<bool>& running;
    AlpacaClient& client;

    MarketGateTask(const TimingConfig& timingCfg,
                   const LoggingConfig& loggingCfg,
                   std::atomic<bool>& allow,
                   std::atomic<bool>& running_flag,
                   AlpacaClient& cli)
        : timing(timingCfg), logging(loggingCfg), allow_fetch(allow), running(running_flag), client(cli) {}
    void operator()();
};

#endif // MARKET_DATA_WORKER_HPP
