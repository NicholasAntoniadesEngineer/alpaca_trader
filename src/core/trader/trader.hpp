#ifndef TRADING_ORCHESTRATOR_HPP
#define TRADING_ORCHESTRATOR_HPP

#include "configs/system_config.hpp"
#include "api/general/api_manager.hpp"
#include "data/account_manager.hpp"
#include "data/data_structures.hpp"
#include "data/data_sync_structures.hpp"
#include "data/market_data_fetcher.hpp"
#include "trading_engine/trading_engine.hpp"
#include "analysis/risk_manager.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/utils/connectivity_manager.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <condition_variable>

namespace AlpacaTrader {
namespace Core {

class TradingOrchestrator {
private:

    struct RuntimeState {
        double initial_equity = 0.0;
        std::thread decision_thread;
        std::atomic<unsigned long> loop_counter{0};
        std::atomic<unsigned long>* iteration_counter = nullptr;
    };

    const SystemConfig& config;
    AccountManager& account_manager;
    TradingEngine trading_engine;
    RiskManager risk_manager;
    MarketDataFetcher data_fetcher;
    DataSyncReferences data_sync;
    RuntimeState runtime;

    double initialize_trading_session();
    void countdown_to_next_cycle();

public:
    TradingOrchestrator(const SystemConfig& cfg, API::ApiManager& api_mgr, AccountManager& account_mgr);
    ~TradingOrchestrator();

    void execute_trading_loop();
    void setup_data_synchronization(const DataSyncConfig& config);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_ORCHESTRATOR_HPP
