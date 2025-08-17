// main.hpp
#ifndef MAIN_HPP
#define MAIN_HPP

#include "Config.hpp"
#include "data/data_structures.hpp"
#include "configs/component_configs.hpp"
#include "api/alpaca_client.hpp"
#include "data/account_manager.hpp"
#include "ui/account_display.hpp"
#include "core/trader.hpp"
#include "configs/trader_config.hpp"
#include "workers/market_data_worker.hpp"
#include "workers/account_data_worker.hpp"
#include "utils/async_logger.hpp"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>

struct SystemState {
    std::mutex mtx;
    std::condition_variable cv;
    MarketSnapshot market;
    AccountSnapshot account;
    std::atomic<bool> has_market{false};
    std::atomic<bool> has_account{false};
    std::atomic<bool> running{true};
    std::atomic<bool> allow_fetch{true};
    Config config; 
    TraderConfig trader_view;

    SystemState() : trader_view(config.strategy, config.risk, config.timing,
                                config.flags, config.ux, config.logging, config.target) {}
    explicit SystemState(const Config& initial)
        : config(initial),
          trader_view(config.strategy, config.risk, config.timing,
                      config.flags, config.ux, config.logging, config.target) {}
};

struct SystemThreads {
    std::thread market;
    std::thread account;
    std::thread gate;
};

struct ComponentConfigBundle {
    AlpacaClientConfig client;
    AccountManagerConfig account_mgr;
    MarketDataTaskConfig market_task;
    AccountDataTaskConfig account_task;
};

struct ComponentInstances {
    std::unique_ptr<AlpacaClient> client;
    std::unique_ptr<AccountManager> account_manager;
    std::unique_ptr<AccountDisplay> account_display;
    std::unique_ptr<Trader> trader;
    std::unique_ptr<MarketDataTask> market_task;
    std::unique_ptr<AccountDataTask> account_task;
    std::unique_ptr<MarketGateTask> market_gate_task;
};

ComponentConfigBundle build_core_configs(const SystemState& state);
ComponentInstances build_core_components(SystemState& state, const ComponentConfigBundle& cfgs);
SystemThreads boot_system(SystemState& system_state, ComponentInstances& comp);
void run_and_shutdown_system(SystemState& system_state, Trader& trader, SystemThreads& handles);
void initialize_application(const Config& config, AsyncLogger& logger);
void run_market_gate(SystemState& state, AlpacaClient& client);

#endif // MAIN_HPP


