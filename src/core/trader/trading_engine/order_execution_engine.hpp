#ifndef ORDER_EXECUTION_ENGINE_HPP
#define ORDER_EXECUTION_ENGINE_HPP

#include "configs/trader_config.hpp"
#include "../data/data_structures.hpp"
#include "../data/data_sync_structures.hpp"
#include "../analysis/strategy_logic.hpp"
#include "api/alpaca_client.hpp"
#include "../data/account_manager.hpp"
#include "core/logging/trading_logs.hpp"
#include <string>
#include <chrono>

namespace AlpacaTrader {
namespace Core {

class OrderExecutionEngine {
public:
    OrderExecutionEngine(API::AlpacaClient& client, AccountManager& account_manager, const TraderConfig& config, DataSyncReferences& data_sync);
    
    void execute_trade(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& sd);
    
    // Order side enumeration
    enum class OrderSide { Buy, Sell };
    
    // Public method for profit taking
    void execute_market_order(OrderSide side, const ProcessedData& data, const StrategyLogic::PositionSizing& sizing);
    
private:
    // Core dependencies
    API::AlpacaClient& client;
    AccountManager& account_manager;
    const TraderConfig& config;
    DataSyncReferences& data_sync;
    
    // Configuration constants
    static constexpr std::chrono::milliseconds POSITION_CLOSE_WAIT_TIME{2000};
    static constexpr int MAX_POSITION_VERIFICATION_ATTEMPTS = 3;
    
    // Core execution methods
    void execute_order(OrderSide side, const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing);
    void execute_bracket_order(OrderSide side, const ProcessedData& data, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::ExitTargets& targets);
    
    // Position management methods
    bool should_close_opposite_position(OrderSide side, int current_qty) const;
    bool close_opposite_position(OrderSide side, int current_qty);
    bool can_execute_new_position(OrderSide side, int current_qty) const;
    
    // Order timing methods
    bool can_place_order_now() const;
    void update_last_order_timestamp();
    
    // Order validation and preparation
    bool validate_order_parameters(const ProcessedData& data, const StrategyLogic::PositionSizing& sizing) const;
    StrategyLogic::ExitTargets calculate_exit_targets(OrderSide side, const ProcessedData& data, const StrategyLogic::PositionSizing& sizing) const;
    
    // Utility methods
    std::string to_side_string(OrderSide side) const;
    std::string to_opposite_side_string(OrderSide side) const;
    bool is_long_position(int qty) const;
    bool is_short_position(int qty) const;
    bool is_flat_position(int qty) const;
    bool should_cancel_existing_orders(const std::string& new_side) const;
    
};

} // namespace Core
} // namespace AlpacaTrader

#endif // ORDER_EXECUTION_ENGINE_HPP
