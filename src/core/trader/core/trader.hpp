#ifndef TRADING_ORCHESTRATOR_HPP
#define TRADING_ORCHESTRATOR_HPP

#include "configs/trader_config.hpp"
#include "api/alpaca_client.hpp"
#include "../data/account_manager.hpp"
#include "../data/data_structures.hpp"
#include "trading_engine.hpp"
#include "risk_manager.hpp"
#include "core/logging/trading_logs.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace AlpacaTrader {
namespace Core {

class TradingOrchestrator {
private:
    struct SharedStateRefs {
        std::mutex* mtx = nullptr;
        std::condition_variable* cv = nullptr;
        MarketSnapshot* market = nullptr;
        AccountSnapshot* account = nullptr;
        std::atomic<bool>* has_market = nullptr;
        std::atomic<bool>* has_account = nullptr;
        std::atomic<bool>* running = nullptr;
        std::atomic<bool>* allow_fetch = nullptr;
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
    SharedStateRefs shared;
    RuntimeState runtime;

    double initialize_trading_session();
    void wait_for_fresh_data();
    std::pair<MarketSnapshot, AccountSnapshot> fetch_current_market_data();
    void display_trading_loop_header();
    void handle_trading_halt();
    void process_trading_cycle(const MarketSnapshot& market, const AccountSnapshot& account);
    void countdown_to_next_cycle();

public:
    TradingOrchestrator(const TraderConfig& cfg, API::AlpacaClient& client_ref, AccountManager& account_mgr);
    ~TradingOrchestrator();

    void execute_trading_loop();
    void attach_shared_state(std::mutex& mtx, std::condition_variable& cv, MarketSnapshot& market, AccountSnapshot& account, std::atomic<bool>& has_market, std::atomic<bool>& has_account, std::atomic<bool>& running_flag, std::atomic<bool>& allow_fetch_flag);
    void start_decision_thread();
    void join_decision_thread();
    void set_iteration_counter(std::atomic<unsigned long>& counter);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_ORCHESTRATOR_HPP
