#ifndef TRADING_ENGINE_HPP
#define TRADING_ENGINE_HPP

#include "configs/trader_config.hpp"
#include "api/alpaca_client.hpp"
#include "api/clock/market_clock.hpp"
#include "../data/account_manager.hpp"
#include "../data/data_structures.hpp"
#include "../analysis/strategy_logic.hpp"
#include "order_execution_engine.hpp"
#include "position_manager.hpp"
#include "trade_validator.hpp"
#include "../analysis/price_manager.hpp"
#include "../data/market_data_fetcher.hpp"
#include "../data/data_sync_structures.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/utils/connectivity_manager.hpp"

namespace AlpacaTrader {
namespace Core {

class TradingEngine {
public:
    TradingEngine(const TraderConfig& config, API::AlpacaClient& client, AccountManager& account_manager);
    
    bool check_trading_permissions(const ProcessedData& data, double equity);
    void execute_trading_decision(const ProcessedData& data, double equity);
    void handle_trading_halt(const std::string& reason);
    bool validate_market_data(const MarketSnapshot& market) const;
    void handle_market_close_positions(const ProcessedData& data);
    void setup_data_synchronization(const DataSyncConfig& config);

private:
    // Core dependencies
    const TraderConfig& config;
    AccountManager& account_manager;
    API::AlpacaClient& client;
    OrderExecutionEngine order_engine;
    PositionManager position_manager;
    TradeValidator trade_validator;
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
    void process_signal_analysis(const ProcessedData& data, double equity);
    void process_position_sizing(const ProcessedData& data, double equity, int current_qty);
    void execute_trade_if_valid(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& signal_decision);
    void check_and_execute_profit_taking(const ProcessedData& data, int current_qty);
    
    // Utility methods
    void perform_halt_countdown(int seconds) const;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_ENGINE_HPP
