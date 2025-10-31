#ifndef TRADING_ORCHESTRATOR_HPP
#define TRADING_ORCHESTRATOR_HPP

#include "configs/system_config.hpp"
#include "api/general/api_manager.hpp"
#include "data/account_manager.hpp"
#include "data/data_structures.hpp"
#include "data/data_sync_structures.hpp"
#include "data/market_data_fetcher.hpp"
#include "trading_engine/trading_engine.hpp"
#include "trading_engine/trading_engine_structures.hpp"
#include "analysis/risk_manager.hpp"
#include "core/logging/logs/trading_logs.hpp"
#include "core/utils/connectivity_manager.hpp"
#include "core/system/system_monitor.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <optional>

namespace AlpacaTrader {
namespace Core {

class TradingOrchestrator {
private:

    struct RuntimeState {
        double initial_equity;
        std::atomic<unsigned long> loop_counter{0};
        std::atomic<unsigned long>* iteration_counter = nullptr;
        
        RuntimeState() : initial_equity(std::numeric_limits<double>::quiet_NaN()) {}
    };

    const SystemConfig& config;
    AccountManager& account_manager;
    TradingEngine trading_engine;
    RiskManager risk_manager;
    MarketDataFetcher data_fetcher;
    std::optional<DataSyncReferences> data_sync;
    std::optional<MarketDataSyncState> fetcher_sync_state;
    RuntimeState runtime;
    ConnectivityManager& connectivity_manager;

    double initialize_trading_session();
    void countdown_to_next_cycle();

public:
    TradingOrchestrator(const TradingOrchestratorConstructionParams& construction_params);

    void execute_trading_loop();
    void setup_data_synchronization(const DataSyncConfig& sync_configuration);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_ORCHESTRATOR_HPP
