#ifndef TRADING_ENGINE_HPP
#define TRADING_ENGINE_HPP

#include "configs/system_config.hpp"
#include "api/general/api_manager.hpp"
#include "core/trader/data/account_manager.hpp"
#include "core/trader/data/data_structures.hpp"
#include "core/trader/analysis/strategy_logic.hpp"
#include "order_execution_engine.hpp"
#include "core/trader/analysis/price_manager.hpp"
#include "core/trader/data/market_data_fetcher.hpp"
#include "core/trader/data/data_sync_structures.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/utils/connectivity_manager.hpp"

namespace AlpacaTrader {
namespace Core {

class TradingEngine {
public:
    TradingEngine(const SystemConfig& config, API::ApiManager& api_manager, AccountManager& account_manager);
    
    bool check_trading_permissions(const ProcessedData& data, double equity);
    void execute_trading_decision(const ProcessedData& data, double equity);
    void handle_trading_halt(const std::string& reason);
    bool validate_market_data(const MarketSnapshot& market) const;
    void setup_data_synchronization(const DataSyncConfig& config);

private:
    // Core dependencies
    const SystemConfig& config;
    AccountManager& account_manager;
    API::ApiManager& api_manager;
    OrderExecutionEngine order_engine;
    PriceManager price_manager;
    MarketDataFetcher data_fetcher;
    
    // Data synchronization references
    DataSyncReferences data_sync;
    
    // Configuration constants
    static constexpr int HALT_SLEEP_SECONDS = 1;
    static constexpr int CONNECTIVITY_RETRY_SECONDS = 1;
    
    // Core validation methods
    bool validate_risk_conditions(const ProcessedData& data, double equity);
    bool check_connectivity();
    bool is_market_open();
    bool is_data_fresh();
    
    // Trading decision methods
    void process_signal_analysis(const ProcessedData& data);
    void process_position_sizing(const ProcessedData& data, double equity, int current_qty);
    void execute_trade_if_valid(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& signal_decision);
    void check_and_execute_profit_taking(const ProcessedData& data, int current_qty);
    
    // Utility methods
    void perform_halt_countdown(int seconds) const;
    bool validate_trade_feasibility(const StrategyLogic::PositionSizing& sizing, double buying_power, double current_price) const;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_ENGINE_HPP
