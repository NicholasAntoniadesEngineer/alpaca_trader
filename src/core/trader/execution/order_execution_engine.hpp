#ifndef ORDER_EXECUTION_ENGINE_HPP
#define ORDER_EXECUTION_ENGINE_HPP

#include "configs/trader_config.hpp"
#include "../data/data_structures.hpp"
#include "../analysis/strategy_logic.hpp"
#include "api/alpaca_client.hpp"
#include "../data/account_manager.hpp"
#include "core/logging/trading_logs.hpp"
#include <string>

namespace AlpacaTrader {
namespace Core {

class OrderExecutionEngine {
public:
    OrderExecutionEngine(API::AlpacaClient& client, AccountManager& account_manager, const TraderConfig& config);
    
    void execute_trade(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& sd);
    
private:
    API::AlpacaClient& client;
    AccountManager& account_manager;
    const TraderConfig& config;
    
    enum class OrderSide { Buy, Sell };
    inline const char* to_side_string(OrderSide s) { return s == OrderSide::Buy ? "buy" : "sell"; }
    
    void execute_buy_order(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing);
    void execute_sell_order(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing);
    
    void handle_position_closure_for_signal(OrderSide side, int current_qty);
    void execute_bracket_order(OrderSide side, const ProcessedData& data, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::ExitTargets& targets);
    void handle_multiple_position_logic(OrderSide side, const ProcessedData& data, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::ExitTargets& targets);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // ORDER_EXECUTION_ENGINE_HPP
