#ifndef TRADING_LOGIC_HPP
#define TRADING_LOGIC_HPP

#include "configs/system_config.hpp"
#include "api/general/api_manager.hpp"
#include "trader/account_management/account_manager.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "trader/strategy_analysis/strategy_logic.hpp"
#include "trader/strategy_analysis/risk_manager.hpp"
#include "order_execution_logic.hpp"
#include "trader/market_data/market_data_fetcher.hpp"
#include "trader/data_structures/data_sync_structures.hpp"
#include "logging/logs/trading_logs.hpp"
#include "utils/connectivity_manager.hpp"
#include "trading_logic_structures.hpp"
#include <memory>
#include <string>

namespace AlpacaTrader {
namespace Core {

class TradingLogic {
public:
    TradingLogic(const TradingLogicConstructionParams& construction_params);
    
    bool check_trading_permissions(const ProcessedData& processed_data_input, double account_equity);
    void execute_trading_decision(const ProcessedData& processed_data_input, double account_equity);
    void execute_trading_cycle(const MarketSnapshot& market_snapshot, const AccountSnapshot& account_snapshot, double initial_equity);
    void handle_trading_halt(const std::string& reason);
    void handle_market_close_positions(const ProcessedData& processed_data_for_close);
    void setup_data_synchronization(const DataSyncConfig& sync_configuration);
    MarketDataFetcher& get_market_data_fetcher_reference();

private:
    // Core dependencies
    const SystemConfig& config;
    AccountManager& account_manager;
    API::ApiManager& api_manager;
    RiskManager risk_manager;
    OrderExecutionLogic order_engine;
    MarketDataFetcher data_fetcher;
    ConnectivityManager& connectivity_manager;
    
    // Data synchronization references - initialized via setup_data_synchronization
    std::unique_ptr<DataSyncReferences> data_sync_ptr;
    
    // Trading decision methods
    void execute_trade_if_valid(const TradeExecutionRequest& trade_request);
    void check_and_execute_profit_taking(const ProfitTakingRequest& profit_taking_request);
    
    // Utility methods
    void perform_halt_countdown(int halt_duration_seconds) const;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_LOGIC_HPP
