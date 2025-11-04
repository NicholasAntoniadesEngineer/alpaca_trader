#ifndef MARKET_GATE_THREAD_HPP
#define MARKET_GATE_THREAD_HPP

#include "configs/timing_config.hpp"
#include "configs/logging_config.hpp"
#include "trader/coordinators/market_gate_coordinator.hpp"
#include <atomic>

namespace AlpacaTrader {
namespace Threads {

struct MarketGateThread {
    const TimingConfig& timing;
    const LoggingConfig& logging;
    std::atomic<bool>& allow_fetch;
    std::atomic<bool>& running;
    AlpacaTrader::Core::MarketGateCoordinator& market_gate_coordinator;
    const std::string& trading_symbol;
    std::atomic<unsigned long>* iteration_counter {nullptr};

    MarketGateThread(const TimingConfig& timing_cfg,
                   const LoggingConfig& logging_cfg,
                   std::atomic<bool>& allow,
                   std::atomic<bool>& running_flag,
                   AlpacaTrader::Core::MarketGateCoordinator& coordinator_ref,
                   const std::string& symbol)
        : timing(timing_cfg), logging(logging_cfg), allow_fetch(allow), running(running_flag), market_gate_coordinator(coordinator_ref), trading_symbol(symbol) {}
    
    void set_iteration_counter(std::atomic<unsigned long>& counter) { iteration_counter = &counter; }
    void operator()();

private:
    void execute_market_gate_monitoring_loop();
};

} // namespace Threads
} // namespace AlpacaTrader

#endif // MARKET_GATE_THREAD_HPP
