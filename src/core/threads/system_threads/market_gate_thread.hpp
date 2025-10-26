#ifndef MARKET_GATE_THREAD_HPP
#define MARKET_GATE_THREAD_HPP

#include "configs/timing_config.hpp"
#include "configs/logging_config.hpp"
#include "api/general/api_manager.hpp"
#include "core/utils/connectivity_manager.hpp"
#include <atomic>

namespace AlpacaTrader {
namespace Threads {

struct MarketGateThread {
    const TimingConfig& timing;
    const LoggingConfig& logging;
    std::atomic<bool>& allow_fetch;
    std::atomic<bool>& running;
    AlpacaTrader::API::ApiManager& api_manager;
    ConnectivityManager& connectivity_manager;
    std::atomic<unsigned long>* iteration_counter {nullptr};

    MarketGateThread(const TimingConfig& timing_cfg,
                   const LoggingConfig& logging_cfg,
                   std::atomic<bool>& allow,
                   std::atomic<bool>& running_flag,
                   AlpacaTrader::API::ApiManager& api_mgr,
                   ConnectivityManager& connectivity_mgr)
        : timing(timing_cfg), logging(logging_cfg), allow_fetch(allow), running(running_flag), api_manager(api_mgr), connectivity_manager(connectivity_mgr) {}
    
    void set_iteration_counter(std::atomic<unsigned long>& counter) { iteration_counter = &counter; }
    void operator()();

private:
    void execute_market_gate_monitoring_loop();
    void check_and_update_fetch_window(bool& last_within_trading_hours);
    void check_and_report_connectivity_status(ConnectivityManager::ConnectionStatus& last_connectivity_status);
};

} // namespace Threads
} // namespace AlpacaTrader

#endif // MARKET_GATE_THREAD_HPP
