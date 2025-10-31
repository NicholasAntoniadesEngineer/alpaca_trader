#ifndef ORDER_EXECUTION_ENGINE_HPP
#define ORDER_EXECUTION_ENGINE_HPP

#include "configs/system_config.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/trader/data/data_sync_structures.hpp"
#include "core/trader/analysis/strategy_logic.hpp"
#include "api/general/api_manager.hpp"
#include "core/trader/data/account_manager.hpp"
#include "core/logging/logs/trading_logs.hpp"
#include "core/system/system_monitor.hpp"
#include "trading_engine_structures.hpp"
#include <string>
#include <chrono>

namespace AlpacaTrader {
namespace Core {

class OrderExecutionEngine {
public:
    OrderExecutionEngine(const OrderExecutionEngineConstructionParams& construction_params);
    
    void execute_trade(const ProcessedData& processed_data_input, int current_position_quantity, const PositionSizing& position_sizing_input, const SignalDecision& signal_decision_input);
    
    // Order side enumeration
    enum class OrderSide { Buy, Sell };
    
        // Public method for profit taking
        void execute_market_order(OrderSide order_side_input, const ProcessedData& processed_data_input, const PositionSizing& position_sizing_input);

        // Public validation method
        bool validate_trade_feasibility(const PositionSizing& position_sizing_input, double buying_power_amount, double current_price_amount) const;

    // Market close position management
    void handle_market_close_positions(const ProcessedData& processed_data_input);
    
    // Data synchronization setup
    void set_data_sync_reference(DataSyncReferences* data_sync_reference);
    
private:
    // Core dependencies
    API::ApiManager& api_manager;
    AccountManager& account_manager;
    const SystemConfig& config;
    DataSyncReferences* data_sync_ptr;
    Monitoring::SystemMonitor& system_monitor;
    
    // Core execution methods
    void execute_order(OrderSide order_side_input, const ProcessedData& processed_data_input, int current_position_quantity, const PositionSizing& position_sizing_input);
    void execute_bracket_order(OrderSide order_side_input, const ProcessedData& processed_data_input, const PositionSizing& position_sizing_input, const ExitTargets& exit_targets_input);
    
    // Position management methods
    bool should_close_opposite_position(OrderSide order_side_input, int current_position_quantity) const;
    bool close_opposite_position(OrderSide order_side_input, int current_position_quantity);
    bool can_execute_new_position(int current_position_quantity) const;
    
    // Order timing methods
    bool can_place_order_now() const;
    void update_last_order_timestamp();
    
    // Order validation and preparation
    bool validate_order_parameters(const ProcessedData& processed_data_input, const PositionSizing& position_sizing_input) const;
    ExitTargets calculate_exit_targets(OrderSide order_side_input, const ProcessedData& processed_data_input, const PositionSizing& position_sizing_input) const;
    
    // Utility methods
    bool is_flat_position(int position_quantity) const;
    bool should_cancel_existing_orders() const;
    
};

} // namespace Core
} // namespace AlpacaTrader

#endif // ORDER_EXECUTION_ENGINE_HPP
