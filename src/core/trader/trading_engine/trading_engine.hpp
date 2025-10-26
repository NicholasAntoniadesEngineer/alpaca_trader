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
#include "core/logging/trading_logs.hpp"
#include "core/utils/connectivity_manager.hpp"
#include "core/system/system_monitor.hpp"

namespace AlpacaTrader {
namespace Core {

class TradingEngine {
public:
    TradingEngine(const SystemConfig& config, API::ApiManager& api_manager, AccountManager& account_manager, Monitoring::SystemMonitor& system_monitor, ConnectivityManager& connectivity_manager);
    
    bool check_trading_permissions(const ProcessedData& data, double equity);
    void execute_trading_decision(const ProcessedData& data, double equity);
    void handle_trading_halt(const std::string& reason);
    void handle_market_close_positions(const ProcessedData& data);

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
    
    // Connectivity check wrapper for risk validation
    bool check_connectivity() const {
        return connectivity_manager.check_connectivity();
    }
    
    // Data synchronization references
    DataSyncReferences data_sync;
    
    // Configuration constants
    static constexpr int HALT_SLEEP_SECONDS = 1;
    static constexpr int CONNECTIVITY_RETRY_SECONDS = 1;
    
    
    // Trading decision methods
    void process_signal_analysis(const ProcessedData& data);
    void process_position_sizing(const ProcessedData& data, double equity, int current_qty);
    void execute_trade_if_valid(const ProcessedData& data, int current_qty, const PositionSizing& sizing, const SignalDecision& signal_decision);
    void check_and_execute_profit_taking(const ProcessedData& data, int current_qty);
    
    // Utility methods
    void perform_halt_countdown(int seconds) const;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_ENGINE_HPP
