#ifndef TRADING_ENGINE_HPP
#define TRADING_ENGINE_HPP

#include "configs/trader_config.hpp"
#include "api/alpaca_client.hpp"
#include "../data/account_manager.hpp"
#include "../data/data_structures.hpp"
#include "../analysis/strategy_logic.hpp"
#include "../execution/order_execution_engine.hpp"
#include "../execution/position_manager.hpp"
#include "../execution/trade_validator.hpp"
#include "../analysis/price_manager.hpp"
#include "../data/market_data_fetcher.hpp"
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
    void process_trading_cycle(const MarketSnapshot& market, const AccountSnapshot& account);

private:
    const TraderConfig& config;
    AccountManager& account_manager;
    OrderExecutionEngine order_engine;
    PositionManager position_manager;
    TradeValidator trade_validator;
    PriceManager price_manager;
    MarketDataFetcher data_fetcher;
    
    bool validate_risk_conditions(const ProcessedData& data, double equity);
    void process_trading_signals(const ProcessedData& data, double equity);
    bool check_market_conditions();
    bool check_connectivity();
    void log_trading_conditions(bool allowed, const std::string& reason);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_ENGINE_HPP
