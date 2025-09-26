#ifndef TRADER_THREAD_HPP
#define TRADER_THREAD_HPP

#include "core/trader.hpp"
#include "configs/timing_config.hpp"
#include <atomic>

namespace AlpacaTrader {
namespace Threads {

struct TraderThread {
    Core::Trader& trader;
    std::atomic<unsigned long>& trader_iterations;
    const TimingConfig& timing;

    TraderThread(Core::Trader& t, std::atomic<unsigned long>& iterations, const TimingConfig& timing_config)
        : trader(t), trader_iterations(iterations), timing(timing_config) {}

    // Thread entrypoint
    void operator()();
};

} // namespace Threads
} // namespace AlpacaTrader

#endif // TRADER_THREAD_HPP
