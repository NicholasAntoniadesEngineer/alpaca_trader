#ifndef MAIN_HPP
#define MAIN_HPP

#include "configs/system_config.hpp"
#include "core/data_structures.hpp"
#include "configs/component_configs.hpp"
#include "configs/trader_config.hpp"
#include "api/alpaca_client.hpp"
#include "core/account_manager.hpp"
#include "logging/account_logger.hpp"
#include "core/trader.hpp"
#include "threads/market_data_thread.hpp"
#include "threads/account_data_thread.hpp"
#include "threads/logging_thread.hpp"
#include "threads/trader_thread.hpp"
#include "threads/thread_manager.hpp"
#include "logging/async_logger.hpp"
#include "utils/connectivity_manager.hpp"
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
    SystemConfig config; 
    TraderConfig trader_view;
    
    // Store trading modules to ensure lifetime throughout execution
    std::unique_ptr<TradingSystemModules> trading_modules;

    SystemState() : trader_view(config.strategy, config.risk, config.timing,
                                config.flags, config.ux, config.logging, config.target) {}
    explicit SystemState(const SystemConfig& initial)
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

struct TradingSystemConfigurations {
    AlpacaClientConfig market_connector;
    AccountManagerConfig portfolio_manager;
    MarketDataThreadConfig market_data_thread;
    AccountDataThreadConfig account_data_thread;
};

struct TradingSystemModules {
    std::unique_ptr<AlpacaClient> market_connector;
    std::unique_ptr<AccountManager> portfolio_manager;
    std::unique_ptr<AccountLogger> account_dashboard;
    std::unique_ptr<Trader> trading_engine;
    std::unique_ptr<MarketDataThread> market_data_thread;
    std::unique_ptr<AccountDataThread> account_data_thread;
    std::unique_ptr<MarketGateThread> market_gate_thread;
    std::unique_ptr<LoggingThread> logging_thread;
    std::unique_ptr<TraderThread> trading_thread;
};

TradingSystemConfigurations build_trading_configurations(const SystemState& state);
TradingSystemModules build_trading_modules(SystemState& state, const TradingSystemConfigurations& configs);
SystemThreads boot_system(SystemState& system_state, TradingSystemModules& modules, AsyncLogger& logger);
void run_and_shutdown_system(SystemState& system_state, SystemThreads& handles);
void initialize_application(const SystemConfig& config, AsyncLogger& logger);
void run_market_gate(SystemState& state, AlpacaClient& client);

#endif // MAIN_HPP


