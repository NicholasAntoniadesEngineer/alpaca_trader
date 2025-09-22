#ifndef TRADER_THREAD_HPP
#define TRADER_THREAD_HPP

#include "../core/trader.hpp"
#include <atomic>

struct TraderThread {
    Trader& trader;
    std::atomic<unsigned long>& trader_iterations;

    TraderThread(Trader& t, std::atomic<unsigned long>& iterations)
        : trader(t), trader_iterations(iterations) {}

    // Thread entrypoint
    void operator()();
};

#endif // TRADER_THREAD_HPP
