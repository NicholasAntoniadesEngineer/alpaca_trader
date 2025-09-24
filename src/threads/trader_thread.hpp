#ifndef TRADER_THREAD_HPP
#define TRADER_THREAD_HPP

#include "core/trader.hpp"
#include <atomic>

namespace AlpacaTrader {
namespace Threads {

struct TraderThread {
    Core::Trader& trader;
    std::atomic<unsigned long>& trader_iterations;

    TraderThread(Core::Trader& t, std::atomic<unsigned long>& iterations)
        : trader(t), trader_iterations(iterations) {}

    // Thread entrypoint
    void operator()();
};

} // namespace Threads
} // namespace AlpacaTrader

#endif // TRADER_THREAD_HPP
