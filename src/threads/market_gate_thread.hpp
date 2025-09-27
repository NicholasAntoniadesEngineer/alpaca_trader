#ifndef MARKET_GATE_THREAD_HPP
#define MARKET_GATE_THREAD_HPP

#include "configs/timing_config.hpp"
#include "configs/logging_config.hpp"
#include "api/alpaca_client.hpp"
#include "utils/connectivity_manager.hpp"
#include <atomic>

namespace AlpacaTrader {
namespace Threads {

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

private:
    // Main business logic methods
    void market_gate_loop();
    void check_and_update_fetch_window(bool& last_within);
    void check_and_report_connectivity_status(ConnectivityManager& connectivity,
                     ConnectivityManager::ConnectionStatus& last_connectivity_status);
};

} // namespace Threads
} // namespace AlpacaTrader

#endif // MARKET_GATE_THREAD_HPP
