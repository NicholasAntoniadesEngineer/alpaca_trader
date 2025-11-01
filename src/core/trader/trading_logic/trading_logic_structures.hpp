#ifndef TRADING_LOGIC_STRUCTURES_HPP
#define TRADING_LOGIC_STRUCTURES_HPP

#include "configs/system_config.hpp"
#include "api/general/api_manager.hpp"
#include "core/trader/account_management/account_manager.hpp"
#include "core/trader/data_structures/data_structures.hpp"
#include "core/system/system_monitor.hpp"
#include "core/utils/connectivity_manager.hpp"
#include "core/trader/data_structures/data_sync_structures.hpp"

namespace AlpacaTrader {
namespace Core {

struct TradingLogicConstructionParams {
    const SystemConfig& system_config;
    API::ApiManager& api_manager_ref;
    AccountManager& account_manager_ref;
    Monitoring::SystemMonitor& system_monitor_ref;
    ConnectivityManager& connectivity_manager_ref;
    
    TradingLogicConstructionParams(const SystemConfig& config, API::ApiManager& api_manager, 
                                   AccountManager& account_manager, Monitoring::SystemMonitor& system_monitor,
                                   ConnectivityManager& connectivity_manager)
        : system_config(config), api_manager_ref(api_manager), account_manager_ref(account_manager),
          system_monitor_ref(system_monitor), connectivity_manager_ref(connectivity_manager) {}
};

struct TradingOrchestratorConstructionParams {
    const SystemConfig& system_config;
    API::ApiManager& api_manager_ref;
    AccountManager& account_manager_ref;
    Monitoring::SystemMonitor& system_monitor_ref;
    ConnectivityManager& connectivity_manager_ref;
    
    TradingOrchestratorConstructionParams(const SystemConfig& config, API::ApiManager& api_manager,
                                        AccountManager& account_manager, Monitoring::SystemMonitor& system_monitor,
                                        ConnectivityManager& connectivity_manager)
        : system_config(config), api_manager_ref(api_manager), account_manager_ref(account_manager),
          system_monitor_ref(system_monitor), connectivity_manager_ref(connectivity_manager) {}
};

struct OrderExecutionLogicConstructionParams {
    API::ApiManager& api_manager_ref;
    AccountManager& account_manager_ref;
    const SystemConfig& system_config;
    DataSyncReferences* data_sync_ptr;
    Monitoring::SystemMonitor& system_monitor_ref;
    
    OrderExecutionLogicConstructionParams(API::ApiManager& api_manager, AccountManager& account_manager,
                                          const SystemConfig& config, DataSyncReferences* data_sync_pointer,
                                          Monitoring::SystemMonitor& system_monitor)
        : api_manager_ref(api_manager), account_manager_ref(account_manager), system_config(config),
          data_sync_ptr(data_sync_pointer), system_monitor_ref(system_monitor) {}
};

struct TradeExecutionRequest {
    const ProcessedData& processed_data;
    int current_position_quantity;
    const PositionSizing& position_sizing;
    const SignalDecision& signal_decision;
    
    TradeExecutionRequest(const ProcessedData& data, int current_position_qty, 
                         const PositionSizing& sizing, const SignalDecision& signal)
        : processed_data(data), current_position_quantity(current_position_qty),
          position_sizing(sizing), signal_decision(signal) {}
};

struct ProfitTakingRequest {
    const ProcessedData& processed_data;
    int current_position_quantity;
    double profit_taking_threshold_dollars;
    
    ProfitTakingRequest(const ProcessedData& data, int current_position_qty, double threshold)
        : processed_data(data), current_position_quantity(current_position_qty),
          profit_taking_threshold_dollars(threshold) {}
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_LOGIC_STRUCTURES_HPP

