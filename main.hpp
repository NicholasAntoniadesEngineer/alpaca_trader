#ifndef MAIN_HPP
#define MAIN_HPP

#include "Config.hpp"
#include "data/data_structures.hpp"
#include "configs/component_configs.hpp"
#include "configs/trader_config.hpp"
#include "api/alpaca_client.hpp"
#include "data/account_manager.hpp"
#include "ui/account_display.hpp"
#include "core/trader.hpp"
#include "threads/market_data_thread.hpp"
#include "threads/account_data_thread.hpp"
#include "threads/logging_thread.hpp"
#include "threads/trader_thread.hpp"
#include "threads/thread_manager.hpp"
#include "utils/async_logger.hpp"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <chrono>

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
    std::thread trader;
    std::thread logger;
    
    // Thread monitoring
    std::chrono::steady_clock::time_point start_time;
    std::atomic<unsigned long> market_iterations{0};
    std::atomic<unsigned long> account_iterations{0};
    std::atomic<unsigned long> gate_iterations{0};
    std::atomic<unsigned long> trader_iterations{0};
    std::atomic<unsigned long> logger_iterations{0};
    
    SystemThreads() : start_time(std::chrono::steady_clock::now()) {}
    
    // Delete copy constructor and copy assignment
    SystemThreads(const SystemThreads&) = delete;
    SystemThreads& operator=(const SystemThreads&) = delete;
    
    // Define move constructor and move assignment
    SystemThreads(SystemThreads&& other) noexcept
        : market(std::move(other.market)),
          account(std::move(other.account)),
          gate(std::move(other.gate)),
          trader(std::move(other.trader)),
          logger(std::move(other.logger)),
          start_time(other.start_time),
          market_iterations(other.market_iterations.load()),
          account_iterations(other.account_iterations.load()),
          gate_iterations(other.gate_iterations.load()),
          trader_iterations(other.trader_iterations.load()),
          logger_iterations(other.logger_iterations.load()) {}
    
    SystemThreads& operator=(SystemThreads&& other) noexcept {
        if (this != &other) {
            market = std::move(other.market);
            account = std::move(other.account);
            gate = std::move(other.gate);
            trader = std::move(other.trader);
            logger = std::move(other.logger);
            start_time = other.start_time;
            market_iterations.store(other.market_iterations.load());
            account_iterations.store(other.account_iterations.load());
            gate_iterations.store(other.gate_iterations.load());
            trader_iterations.store(other.trader_iterations.load());
            logger_iterations.store(other.logger_iterations.load());
        }
        return *this;
    }
};

struct ComponentConfigBundle {
    AlpacaClientConfig client;
    AccountManagerConfig account_mgr;
    MarketDataThreadConfig market_thread;
    AccountDataThreadConfig account_thread;
};

struct ComponentInstances {
    std::unique_ptr<AlpacaClient> client;
    std::unique_ptr<AccountManager> account_manager;
    std::unique_ptr<AccountDisplay> account_display;
    std::unique_ptr<Trader> trader;
    std::unique_ptr<MarketDataThread> market_thread;
    std::unique_ptr<AccountDataThread> account_thread;
    std::unique_ptr<MarketGateThread> market_gate_thread;
    std::unique_ptr<LoggingThread> logging_thread;
    std::unique_ptr<TraderThread> trader_thread;
};

ComponentConfigBundle build_core_configs(const SystemState& state);
ComponentInstances build_core_components(SystemState& state, const ComponentConfigBundle& cfgs);
SystemThreads boot_system(SystemState& system_state, ComponentInstances& comp, AsyncLogger& logger);
void run_and_shutdown_system(SystemState& system_state, SystemThreads& handles);
void initialize_application(const Config& config, AsyncLogger& logger);
void run_market_gate(SystemState& state, AlpacaClient& client);

#endif // MAIN_HPP


