#ifndef TRADING_ENGINE_HPP
#define TRADING_ENGINE_HPP

#include "configs/system_config.hpp"
#include "api/general/api_manager.hpp"
#include "core/trader/data/account_manager.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/trader/analysis/strategy_logic.hpp"
#include "core/trader/analysis/risk_manager.hpp"
#include "core/trader/analysis/signal_processor.hpp"
#include "order_execution_engine.hpp"
#include "core/trader/data/market_data_fetcher.hpp"
#include "core/trader/data/data_sync_structures.hpp"
#include "core/logging/logs/trading_logs.hpp"
#include "core/utils/connectivity_manager.hpp"
#include "core/system/system_monitor.hpp"
#include "trading_engine_structures.hpp"

namespace AlpacaTrader {
namespace Core {

class TradingEngine {
public:
    TradingEngine(const TradingEngineConstructionParams& construction_params);
    
    bool check_trading_permissions(const ProcessedData& processed_data_input, double account_equity);
    void execute_trading_decision(const ProcessedData& processed_data_input, double account_equity);
    void handle_trading_halt(const std::string& reason);
    void handle_market_close_positions(const ProcessedData& processed_data_for_close);
    void setup_data_synchronization(const DataSyncConfig& sync_configuration);

private:
    // Core dependencies
    const SystemConfig& config;
    AccountManager& account_manager;
    API::ApiManager& api_manager;
    RiskManager risk_manager;
    SignalProcessor signal_processor;
    OrderExecutionEngine order_engine;
    MarketDataFetcher data_fetcher;
    Monitoring::SystemMonitor& system_monitor;
    ConnectivityManager& connectivity_manager;
    
    // Data synchronization references - initialized via setup_data_synchronization
    std::unique_ptr<DataSyncReferences> data_sync_ptr;
    
    // Trading decision methods
    void process_signal_analysis(const ProcessedData& processed_data_input);
    void process_position_sizing(const ProcessedData& processed_data_input, double account_equity_amount, int current_position_quantity);
    void execute_trade_if_valid(const TradeExecutionRequest& trade_request);
    void check_and_execute_profit_taking(const ProfitTakingRequest& profit_taking_request);
    
    // Utility methods
    void perform_halt_countdown(int halt_duration_seconds) const;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_ENGINE_HPP
