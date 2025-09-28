// component_configs.cpp
#include "configs/component_configs.hpp"
#include "core/system/system_state.hpp"
#include "core/system/system_configurations.hpp"
#include "core/system/system_modules.hpp"
#include "api/alpaca_client.hpp"
#include "core/trader/account_manager.hpp"
#include "core/logging/account_logs.hpp"
#include "core/trader/trader.hpp"
#include "core/threads/system_threads/market_data_thread.hpp"
#include "core/threads/system_threads/account_data_thread.hpp"

// =============================================================================
// TRADING SYSTEM MODULE CONFIGURATION CREATION
// =============================================================================

SystemConfigurations create_trading_configurations(const SystemState& state) {
    return SystemConfigurations{
        AlpacaClientConfig{state.config.api, state.config.session, state.config.logging, state.config.target, state.config.timing, state.config.orders},
        AccountManagerConfig{state.config.api, state.config.logging, state.config.target, state.config.timing},
        MarketDataThreadConfig{state.config.strategy, state.config.timing, state.config.target},
        AccountDataThreadConfig{state.config.timing}
    };
}

// =============================================================================
// TRADING SYSTEM MODULE INSTANCE CREATION
// =============================================================================

SystemModules create_trading_modules(SystemState& state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger) {
    SystemConfigurations configs = create_trading_configurations(state);
    SystemModules modules;
    
    // Create core trading modules
    modules.market_connector = std::make_unique<AlpacaTrader::API::AlpacaClient>(configs.market_connector);
    modules.portfolio_manager = std::make_unique<AlpacaTrader::Core::AccountManager>(configs.portfolio_manager);
    modules.account_dashboard = std::make_unique<AlpacaTrader::Logging::AccountLogs>(state.config.logging, *modules.portfolio_manager);
    modules.trading_engine = std::make_unique<AlpacaTrader::Core::Trader>(state.trader_view, *modules.market_connector, *modules.portfolio_manager);
    
    // Create thread modules
    
    // Create MARKET_DATA thread
    modules.market_data_thread = std::make_unique<AlpacaTrader::Threads::MarketDataThread>(configs.market_data_thread, *modules.market_connector, 
                                                                   state.mtx, state.cv, state.market, 
                                                                   state.has_market, state.running);
    
    // Create ACCOUNT_DATA thread
    modules.account_data_thread = std::make_unique<AlpacaTrader::Threads::AccountDataThread>(configs.account_data_thread, *modules.portfolio_manager, 
                                                                     state.mtx, state.cv, state.account, 
                                                                     state.has_account, state.running);
    
    // Create MARKET_GATE thread
    modules.market_gate_thread = std::make_unique<AlpacaTrader::Threads::MarketGateThread>(state.config.timing, state.config.logging, 
                                                                   state.allow_fetch, state.running, *modules.market_connector);
    
    // Create LOGGING thread
    static std::atomic<unsigned long> logging_iterations{0};
    modules.logging_thread = std::make_unique<AlpacaTrader::Threads::LoggingThread>(logger, logging_iterations, state.config);
    
    // Create TRADER_DECISION thread
    static std::atomic<unsigned long> trader_iterations{0};
    modules.trading_thread = std::make_unique<AlpacaTrader::Threads::TraderThread>(*modules.trading_engine, trader_iterations, state.config.timing);
    
    return modules;
}

// =============================================================================
// TRADING SYSTEM MODULE CONFIGURATION
// =============================================================================

void configure_trading_modules(SystemState& system_state, SystemModules& modules) {
    // Configure trading engine with shared state
    modules.trading_engine->attach_shared_state(system_state.mtx, system_state.cv, system_state.market, 
                                              system_state.account, system_state.has_market, 
                                              system_state.has_account, system_state.running, system_state.allow_fetch);
    modules.trading_engine->start_decision_thread();
    
    // Configure thread modules
    modules.market_data_thread->set_allow_fetch_flag(system_state.allow_fetch);
    modules.account_data_thread->set_allow_fetch_flag(system_state.allow_fetch);
}
