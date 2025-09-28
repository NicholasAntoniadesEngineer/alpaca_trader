#ifndef TRADER_THREAD_HPP
#define TRADER_THREAD_HPP

#include "configs/timing_config.hpp"
#include "core/trader/core/trader.hpp"
#include <atomic>

namespace AlpacaTrader {
namespace Threads {

struct TraderThread {
    AlpacaTrader::Core::TradingOrchestrator& trader;
    std::atomic<unsigned long>* trader_iterations;
    const TimingConfig& timing;

    TraderThread(AlpacaTrader::Core::TradingOrchestrator& t, std::atomic<unsigned long>& iterations, const TimingConfig& timing_config)
        : trader(t), trader_iterations(&iterations), timing(timing_config) {}

    // Thread entrypoint
    void operator()();
    void set_iteration_counter(std::atomic<unsigned long>& counter) { trader_iterations = &counter; }
};

} // namespace Threads
} // namespace AlpacaTrader

#endif // TRADER_THREAD_HPP
