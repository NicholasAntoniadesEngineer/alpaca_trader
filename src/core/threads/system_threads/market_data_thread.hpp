#ifndef MARKET_DATA_THREAD_HPP
#define MARKET_DATA_THREAD_HPP

#include "configs/strategy_config.hpp"
#include "configs/timing_config.hpp"
#include "core/threads/thread_register.hpp"
#include "api/general/api_manager.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/logging/async_logger.hpp"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace AlpacaTrader {
namespace Threads {

using MarketDataThreadConfig = AlpacaTrader::Config::MarketDataThreadConfig;

struct MarketDataThread {
    const StrategyConfig& strategy;  // Now contains target settings
    const TimingConfig& timing;
    AlpacaTrader::API::ApiManager& api_manager;
    std::mutex& state_mtx;
    std::condition_variable& data_cv;
    AlpacaTrader::Core::MarketSnapshot& market_snapshot;
    std::atomic<bool>& has_market;
    std::atomic<bool>& running;
    std::atomic<std::chrono::steady_clock::time_point>& market_data_timestamp;
    std::atomic<bool>& market_data_fresh;

    // For tracking previous bar data to detect changes
    AlpacaTrader::Core::Bar previous_bar{};
    std::chrono::steady_clock::time_point last_bar_log_time{};
    std::atomic<bool>* allow_fetch_ptr {nullptr};
    std::atomic<unsigned long>* iteration_counter {nullptr};

    MarketDataThread(const MarketDataThreadConfig& cfg,
                    AlpacaTrader::API::ApiManager& api_mgr,
                    std::mutex& mtx,
                    std::condition_variable& cv,
                    AlpacaTrader::Core::MarketSnapshot& snapshot,
                   std::atomic<bool>& has_market_flag,
                   std::atomic<bool>& running_flag,
                   std::atomic<std::chrono::steady_clock::time_point>& timestamp,
                   std::atomic<bool>& fresh_flag)
        : strategy(cfg.strategy), timing(cfg.timing), api_manager(api_mgr),
          state_mtx(mtx), data_cv(cv), market_snapshot(snapshot), has_market(has_market_flag),
          running(running_flag), market_data_timestamp(timestamp), market_data_fresh(fresh_flag) {}

    // External gate: when set, task will fetch; otherwise it sleeps
    void set_allow_fetch_flag(std::atomic<bool>& allow_flag) { allow_fetch_ptr = &allow_flag; }
    
    // Set iteration counter for monitoring
    void set_iteration_counter(std::atomic<unsigned long>& counter) { iteration_counter = &counter; }

    // Thread entrypoint
    void operator()();

private:

    void market_data_loop();
    void fetch_and_process_market_data();
    void update_market_snapshot(const AlpacaTrader::Core::ProcessedData& computed);
};


} // namespace Threads
} // namespace AlpacaTrader

#endif // MARKET_DATA_THREAD_HPP
