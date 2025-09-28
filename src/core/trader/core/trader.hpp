#ifndef TRADING_ORCHESTRATOR_HPP
#define TRADING_ORCHESTRATOR_HPP

#include "configs/trader_config.hpp"
#include "api/alpaca_client.hpp"
#include "../data/account_manager.hpp"
#include "../data/data_structures.hpp"
#include "../data/market_data_fetcher.hpp"
#include "trading_engine.hpp"
#include "risk_manager.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/utils/connectivity_manager.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace AlpacaTrader {
namespace Core {

class TradingOrchestrator {
private:
        struct DataSyncReferences {
            std::mutex* mtx = nullptr;
            std::condition_variable* cv = nullptr;
            MarketSnapshot* market = nullptr;
            AccountSnapshot* account = nullptr;
            std::atomic<bool>* has_market = nullptr;
            std::atomic<bool>* has_account = nullptr;
            std::atomic<bool>* running = nullptr;
            std::atomic<bool>* allow_fetch = nullptr;
            
            // Default constructor
            DataSyncReferences() = default;
            
            // Constructor for easy initialization from DataSyncConfig
            DataSyncReferences(const DataSyncConfig& config)
                : mtx(&config.mtx), cv(&config.cv), market(&config.market), account(&config.account),
                  has_market(&config.has_market), has_account(&config.has_account),
                  running(&config.running), allow_fetch(&config.allow_fetch) {}
        };

    struct RuntimeState {
        double initial_equity = 0.0;
        std::thread decision_thread;
        std::atomic<unsigned long> loop_counter{0};
        std::atomic<unsigned long>* iteration_counter = nullptr;
    };

    const TraderConfig& config;
    AccountManager& account_manager;
    TradingEngine trading_engine;
    RiskManager risk_manager;
    MarketDataFetcher data_fetcher;
    DataSyncReferences data_sync;
    RuntimeState runtime;

    double initialize_trading_session();
    void countdown_to_next_cycle();
    bool check_connectivity_status();

public:
    TradingOrchestrator(const TraderConfig& cfg, API::AlpacaClient& client_ref, AccountManager& account_mgr);
    ~TradingOrchestrator();

    void execute_trading_loop();
    void setup_data_synchronization(const DataSyncConfig& config);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_ORCHESTRATOR_HPP
