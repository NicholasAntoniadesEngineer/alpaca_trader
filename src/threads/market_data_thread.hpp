#ifndef MARKET_DATA_THREAD_HPP
#define MARKET_DATA_THREAD_HPP

#include "configs/strategy_config.hpp"
#include "configs/timing_config.hpp"
#include "configs/target_config.hpp"
#include "api/alpaca_client.hpp"
#include "configs/component_configs.hpp"
#include "core/data_structures.hpp"
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace AlpacaTrader {
namespace Threads {

struct MarketDataThread {
    const StrategyConfig& strategy;
    const TimingConfig& timing;
    const TargetConfig& target;
    API::AlpacaClient& client;
    std::mutex& state_mtx;
    std::condition_variable& data_cv;
    Core::MarketSnapshot& market_snapshot;
    std::atomic<bool>& has_market;
    std::atomic<bool>& running;
    std::atomic<bool>* allow_fetch_ptr {nullptr};
    std::atomic<unsigned long>* iteration_counter {nullptr};

    MarketDataThread(const MarketDataThreadConfig& cfg,
                    API::AlpacaClient& cli,
                    std::mutex& mtx,
                    std::condition_variable& cv,
                    Core::MarketSnapshot& snapshot,
                   std::atomic<bool>& has_market_flag,
                   std::atomic<bool>& running_flag)
        : strategy(cfg.strategy), timing(cfg.timing), target(cfg.target), client(cli),
          state_mtx(mtx), data_cv(cv), market_snapshot(snapshot), has_market(has_market_flag),
          running(running_flag) {}

    // External gate: when set, task will fetch; otherwise it sleeps
    void set_allow_fetch_flag(std::atomic<bool>& allow_flag) { allow_fetch_ptr = &allow_flag; }
    
    // Set iteration counter for monitoring
    void set_iteration_counter(std::atomic<unsigned long>& counter) { iteration_counter = &counter; }

    // Thread entrypoint
    void operator()();
};

void run_market_gate(std::atomic<bool>& running,
                     std::atomic<bool>& allow_fetch,
                     const TimingConfig& timing,
                     const LoggingConfig& logging,
                      API::AlpacaClient& client,
                     std::atomic<unsigned long>* iteration_counter = nullptr);

struct MarketGateThread {
    const TimingConfig& timing;
    const LoggingConfig& logging;
    std::atomic<bool>& allow_fetch;
    std::atomic<bool>& running;
    API::AlpacaClient& client;
    std::atomic<unsigned long>* iteration_counter {nullptr};

    MarketGateThread(const TimingConfig& timing_cfg,
                   const LoggingConfig& logging_cfg,
                   std::atomic<bool>& allow,
                   std::atomic<bool>& running_flag,
                   API::AlpacaClient& cli)
        : timing(timing_cfg), logging(logging_cfg), allow_fetch(allow), running(running_flag), client(cli) {}
    
    void set_iteration_counter(std::atomic<unsigned long>& counter) { iteration_counter = &counter; }
    void operator()();
};

} // namespace Threads
} // namespace AlpacaTrader

#endif // MARKET_DATA_THREAD_HPP
