// Trader.hpp
#ifndef TRADER_HPP
#define TRADER_HPP

#include "configs/trader_config.hpp"
#include "api/alpaca_client.hpp"
#include "core/account_manager.hpp"
#include "core/data_structures.hpp"
#include "core/strategy_logic.hpp"
#include "logging/trading_logs.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace AlpacaTrader {
namespace Core {

class Trader {
private:
    struct TraderServices {
        const TraderConfig& config;
        API::AlpacaClient& client;
        AccountManager& account_manager;
    };

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

    TraderServices services;
    SharedStateRefs shared;
    RuntimeState runtime;

    // Initialization and single-shot helpers
    double initialize_trader();

    // Building blocks
    ProcessedData fetch_and_process_data();
    bool can_trade(double exposure_pct);
    void evaluate_and_execute_signal(const ProcessedData& data, double equity);

    // Helper methods for readable decision loop
    void wait_for_fresh_data();
    std::pair<MarketSnapshot, AccountSnapshot> get_current_snapshots();
    void display_loop_header();
    void handle_trading_halt();
    void process_trading_cycle(const MarketSnapshot& market, const AccountSnapshot& account);
    void countdown_to_next_cycle();

    // Signal evaluation breakdown (now inlined)
    bool validate_trade_feasibility(const StrategyLogic::PositionSizing& sizing, double buying_power, double current_price) const;
    
    // Real-time pricing helpers
    double get_real_time_price_with_fallback(double fallback_price) const;
    
    // Market close position management
    void handle_market_close_positions(const ProcessedData& data);
    
    void execute_trade(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& sd);

public:
    Trader(const TraderConfig& cfg, API::AlpacaClient& client_ref, AccountManager& account_mgr);
    ~Trader();

    // Expose decision loop for external thread management
    
    // Direct access to decision loop (for thread consistency)
    void decision_loop();

    // Configure shared state pointers from main
    void attach_shared_state(std::mutex& mtx, std::condition_variable& cv,  MarketSnapshot& market, AccountSnapshot& account, std::atomic<bool>& has_market, std::atomic<bool>& has_account, std::atomic<bool>& running_flag, std::atomic<bool>& allow_fetch_flag);

    // Start only the decision thread
    void start_decision_thread();
    void join_decision_thread();
    
    // Set iteration counter for monitoring
    void set_iteration_counter(std::atomic<unsigned long>& counter);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADER_HPP
