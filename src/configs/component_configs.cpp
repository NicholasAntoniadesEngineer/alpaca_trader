// component_configs.cpp
#include "configs/component_configs.hpp"
#include "core/system_state.hpp"
#include "core/trading_system_configurations.hpp"
#include "core/trading_system_modules.hpp"
#include "api/alpaca_client.hpp"
#include "core/account_manager.hpp"
#include "logging/account_logger.hpp"
#include "core/trader.hpp"
#include "threads/market_data_thread.hpp"
#include "threads/account_data_thread.hpp"

// =============================================================================
// TRADING SYSTEM MODULE CONFIGURATION CREATION
// =============================================================================

TradingSystemConfigurations create_trading_configurations(const SystemState& state) {
    return TradingSystemConfigurations{
        AlpacaClientConfig{state.config.api, state.config.session, state.config.logging, state.config.target, state.config.timing},
        AccountManagerConfig{state.config.api, state.config.logging, state.config.target, state.config.timing},
        MarketDataThreadConfig{state.config.strategy, state.config.timing, state.config.target},
        AccountDataThreadConfig{state.config.timing}
    };
}

// =============================================================================
// TRADING SYSTEM MODULE INSTANCE CREATION
// =============================================================================

TradingSystemModules create_trading_modules(SystemState& state) {
    TradingSystemConfigurations configs = create_trading_configurations(state);
    TradingSystemModules modules;
    
    // Create core trading modules
    modules.market_connector = std::make_unique<AlpacaTrader::API::AlpacaClient>(configs.market_connector);
    modules.portfolio_manager = std::make_unique<AlpacaTrader::Core::AccountManager>(configs.portfolio_manager);
    modules.account_dashboard = std::make_unique<AlpacaTrader::Logging::AccountLogger>(state.config.logging, *modules.portfolio_manager);
    modules.trading_engine = std::make_unique<AlpacaTrader::Core::Trader>(state.trader_view, *modules.market_connector, *modules.portfolio_manager);
    
    // Create thread modules (but not thread objects yet)
    modules.market_data_thread = std::make_unique<AlpacaTrader::Threads::MarketDataThread>(configs.market_data_thread, *modules.market_connector, 
                                                                   state.mtx, state.cv, state.market, 
                                                                   state.has_market, state.running);
    modules.account_data_thread = std::make_unique<AlpacaTrader::Threads::AccountDataThread>(configs.account_data_thread, *modules.portfolio_manager, 
                                                                     state.mtx, state.cv, state.account, 
                                                                     state.has_account, state.running);
    modules.market_gate_thread = std::make_unique<AlpacaTrader::Threads::MarketGateThread>(state.config.timing, state.config.logging, 
                                                                    state.allow_fetch, state.running, *modules.market_connector);
    
    // Thread objects will be created in thread manager
    modules.logging_thread = nullptr;
    modules.trading_thread = nullptr;
    
    return modules;
}

// =============================================================================
// TRADING SYSTEM MODULE CONFIGURATION
// =============================================================================

void configure_trading_modules(SystemState& system_state, TradingSystemModules& modules) {
    // Configure trading engine with shared state
    modules.trading_engine->attach_shared_state(system_state.mtx, system_state.cv, system_state.market, 
                                              system_state.account, system_state.has_market, 
                                              system_state.has_account, system_state.running);
    modules.trading_engine->start_decision_thread();
    
    // Configure thread modules
    modules.market_data_thread->set_allow_fetch_flag(system_state.allow_fetch);
    modules.account_data_thread->set_allow_fetch_flag(system_state.allow_fetch);
}
