#ifndef TRADING_LOGIC_STRUCTURES_HPP
#define TRADING_LOGIC_STRUCTURES_HPP

#include "configs/system_config.hpp"
#include "api/general/api_manager.hpp"
#include "trader/account_management/account_manager.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "utils/connectivity_manager.hpp"
#include "trader/data_structures/data_sync_structures.hpp"

namespace AlpacaTrader {
namespace Core {

struct TradingLogicConstructionParams {
    const SystemConfig& system_config;
    API::ApiManager& api_manager_ref;
    AccountManager& account_manager_ref;
    ConnectivityManager& connectivity_manager_ref;
    
    TradingLogicConstructionParams(const SystemConfig& config, API::ApiManager& api_manager, 
                                   AccountManager& account_manager,
                                   ConnectivityManager& connectivity_manager)
        : system_config(config), api_manager_ref(api_manager), account_manager_ref(account_manager),
          connectivity_manager_ref(connectivity_manager) {}
};

struct TradingOrchestratorConstructionParams {
    const SystemConfig& system_config;
    API::ApiManager& api_manager_ref;
    AccountManager& account_manager_ref;
    ConnectivityManager& connectivity_manager_ref;
    
    TradingOrchestratorConstructionParams(const SystemConfig& config, API::ApiManager& api_manager,
                                        AccountManager& account_manager,
                                        ConnectivityManager& connectivity_manager)
        : system_config(config), api_manager_ref(api_manager), account_manager_ref(account_manager),
          connectivity_manager_ref(connectivity_manager) {}
};

struct OrderExecutionLogicConstructionParams {
    API::ApiManager& api_manager_ref;
    AccountManager& account_manager_ref;
    const SystemConfig& system_config;
    DataSyncReferences* data_sync_ptr;
    
    OrderExecutionLogicConstructionParams(API::ApiManager& api_manager, AccountManager& account_manager,
                                          const SystemConfig& config, DataSyncReferences* data_sync_pointer)
        : api_manager_ref(api_manager), account_manager_ref(account_manager), system_config(config),
          data_sync_ptr(data_sync_pointer) {}
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

