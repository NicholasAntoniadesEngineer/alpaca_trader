#ifndef MARKET_DATA_THREAD_HPP
#define MARKET_DATA_THREAD_HPP

#include "configs/strategy_config.hpp"
#include "configs/timing_config.hpp"
#include "configs/target_config.hpp"
#include "api/alpaca_client.hpp"
#include "configs/component_configs.hpp"
#include "core/trader/data/data_structures.hpp"
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

private:

    void market_data_loop();
    void fetch_and_process_market_data();
    void update_market_snapshot(const Core::ProcessedData& computed);
};


} // namespace Threads
} // namespace AlpacaTrader

#endif // MARKET_DATA_THREAD_HPP
