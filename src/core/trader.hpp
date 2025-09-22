// Trader.hpp
#ifndef TRADER_HPP
#define TRADER_HPP

#include "../configs/trader_config.hpp"
#include "../api/alpaca_client.hpp"
#include "../account_manager/account_manager.hpp"
#include "../utils/data_structures.hpp"
#include "strategy_logic.hpp"
#include "../logging/trading_logger.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

class Trader {
private:
    struct TraderServices {
        const TraderConfig& config;
        AlpacaClient& client;
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

    // Decision thread loop (moved to public for thread consistency)

    // Building blocks
    ProcessedData fetch_and_process_data();
    bool can_trade(double exposure_pct);
    void evaluate_and_execute_signal(const ProcessedData& data, double equity);

    // Helper methods for readable decision loop
    void wait_for_fresh_data();
    std::pair<MarketSnapshot, AccountSnapshot> get_current_snapshots();
    void display_loop_header();
    void handle_trading_halt();
    void display_equity_status(double equity);
    void process_trading_cycle(const MarketSnapshot& market, const AccountSnapshot& account);
    void countdown_to_next_cycle();

    // Signal evaluation breakdown
    StrategyLogic::SignalDecision detect_signals(const ProcessedData& data) const;
    StrategyLogic::FilterResult evaluate_filters(const ProcessedData& data) const;
    StrategyLogic::PositionSizing calculate_position_sizing(const ProcessedData& data, double equity, int current_qty, double buying_power = 0.0) const;
    bool validate_trade_feasibility(const StrategyLogic::PositionSizing& sizing, double buying_power, double current_price) const;
    
    // Real-time pricing helpers
    double get_real_time_price_with_fallback(double fallback_price) const;
    void log_exit_target_debug(const std::string& side, double price, double risk, double rr, const StrategyLogic::ExitTargets& targets) const;
    
    void execute_trade(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& sd);

public:
    Trader(const TraderConfig& cfg, AlpacaClient& clientRef, AccountManager& accountMgr);
    ~Trader();

    // Print header/config only
    void run();

    // Expose decision loop for external thread management
    void run_decision_loop();
    
    // Direct access to decision loop (for thread consistency)
    void decision_loop();

    // Configure shared state pointers from main
    void attach_shared_state(std::mutex& mtx, std::condition_variable& cv,  MarketSnapshot& market, AccountSnapshot& account, std::atomic<bool>& has_market, std::atomic<bool>& has_account, std::atomic<bool>& running_flag);

    // Start only the decision thread
    void start_decision_thread();
    void join_decision_thread();
    
    // Set iteration counter for monitoring
    void set_iteration_counter(std::atomic<unsigned long>& counter);
};

#endif // TRADER_HPP
