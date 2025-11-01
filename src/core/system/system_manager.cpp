#include "system_manager.hpp"
#include "system_state.hpp"
#include "system_modules.hpp"
#include "system_threads.hpp"
#include "core/threads/thread_logic/thread_registry.hpp"
#include "core/threads/thread_logic/thread_manager.hpp"
#include "core/logging/logs/startup_logs.hpp"
#include "core/logging/logger/async_logger.hpp"
#include "core/logging/logs/account_logs.hpp"
#include "configs/system_config.hpp"
#include "core/system/system_configurations.hpp"
#include "core/trader/coordinators/trading_coordinator.hpp"
#include "core/trader/account_management/account_manager.hpp"
#include "core/trader/market_data/market_data_fetcher.hpp"
#include "core/trader/trading_logic/trading_logic.hpp"
#include "core/trader/trading_logic/trading_logic_structures.hpp"
#include "core/trader/strategy_analysis/risk_manager.hpp"
#include "api/general/api_manager.hpp"
#include "core/threads/system_threads/logging_thread.hpp"
#include "core/threads/system_threads/market_data_thread.hpp"
#include "core/threads/system_threads/account_data_thread.hpp"
#include "core/threads/system_threads/market_gate_thread.hpp"
#include "core/threads/system_threads/trader_thread.hpp"
#include "core/trader/data_structures/data_structures.hpp"
#include "core/threads/thread_register.hpp"
#include "core/logging/logs/thread_logs.hpp"
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <thread>

using namespace AlpacaTrader::Core;
using namespace AlpacaTrader::Logging;
using namespace AlpacaTrader::Threads;



SystemConfigurations create_trading_configurations(const SystemState& state) {
    return SystemConfigurations{
        AccountManagerConfig{state.config.logging, state.config.timing, state.config.strategy},
        MarketDataThreadConfig{state.config.strategy, state.config.timing},
        AccountDataThreadConfig{state.config.timing}
    };
}

SystemModules create_trading_modules(SystemState& state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger, SystemThreads& thread_handles) {
    SystemConfigurations configs = create_trading_configurations(state);
    SystemModules modules;

    // Create API manager 
    modules.api_manager = std::make_unique<AlpacaTrader::API::ApiManager>(state.config.multi_api, state.connectivity_manager);
    
    // Create account manager
    AlpacaTrader::Config::AccountManagerConfig account_config{
        state.config.logging, state.config.timing, state.config.strategy
    };
    modules.portfolio_manager = std::make_unique<AlpacaTrader::Core::AccountManager>(account_config, *modules.api_manager);
    
    // Create trading logic (creates its own MarketDataFetcher internally)
    TradingLogicConstructionParams trading_logic_params(state.config, *modules.api_manager, *modules.portfolio_manager, state.system_monitor, state.connectivity_manager);
    modules.trading_logic = std::make_unique<AlpacaTrader::Core::TradingLogic>(trading_logic_params);
    
    // Create trading coordinator - uses MarketDataFetcher from TradingLogic
    modules.trading_coordinator = std::make_unique<AlpacaTrader::Core::TradingCoordinator>(
        *modules.trading_logic,
        modules.trading_logic->get_market_data_fetcher_reference(),
        state.connectivity_manager,
        *modules.portfolio_manager,
        state.config
    );
    
    modules.account_dashboard = std::make_unique<AlpacaTrader::Logging::AccountLogs>(state.config.logging, *modules.portfolio_manager, state.config.strategy.position_long_string, state.config.strategy.position_short_string);
    
    // Create coordinator interfaces for thread access to trader components
    modules.market_data_coordinator = std::make_unique<AlpacaTrader::Core::MarketDataCoordinator>(*modules.api_manager, state.config);
    modules.account_data_coordinator = std::make_unique<AlpacaTrader::Core::AccountDataCoordinator>(*modules.portfolio_manager);
    
    // Create thread modules
    
    // Create MARKET_DATA thread
    modules.market_data_thread = std::make_unique<AlpacaTrader::Threads::MarketDataThread>(configs.market_data_thread, *modules.market_data_coordinator,
                                                                   state.mtx, state.cv, state.market,
                                                                   state.has_market, state.running,
                                                                   state.market_data_timestamp, state.market_data_fresh);
    
    // Create ACCOUNT_DATA thread
    modules.account_data_thread = std::make_unique<AlpacaTrader::Threads::AccountDataThread>(configs.account_data_thread, *modules.account_data_coordinator, 
                                                                     state.mtx, state.cv, state.account,
                                                                     state.has_account, state.running);
    
    // Create MARKET_GATE thread
    modules.market_gate_thread = std::make_unique<AlpacaTrader::Threads::MarketGateThread>(state.config.timing, state.config.logging,
                                                                   state.allow_fetch, state.running, *modules.api_manager, state.connectivity_manager, state.config.trading_mode.primary_symbol);
    
    // Create LOGGING thread
    modules.logging_thread = std::make_unique<AlpacaTrader::Threads::LoggingThread>(logger, thread_handles.logger_iterations, state.config);
    
    // Get initial equity for trader thread
    double initial_equity = modules.portfolio_manager->fetch_account_equity();
    if (initial_equity <= 0.0 || !std::isfinite(initial_equity)) {
        throw std::runtime_error("Failed to get initial equity for trader thread");
    }
    
    // Create TRADER_DECISION thread
    modules.trading_thread = std::make_unique<AlpacaTrader::Threads::TraderThread>(
        state.config.timing,
        *modules.trading_coordinator,
        state.mtx, state.cv,
        state.market, state.account,
        state.has_market, state.has_account, state.running,
        state.market_data_timestamp, state.market_data_fresh,
        state.last_order_timestamp,
        initial_equity
    );
    
    return modules;
}

void configure_trading_modules(SystemThreads& handles, SystemModules& modules, SystemState& state) {
    // Configure thread iteration counters using the generic registry approach
    AlpacaTrader::Core::ThreadRegistry::configure_thread_iteration_counters(handles, modules);
    
    // Configure allow_fetch_ptr for threads that need it
    if (modules.market_data_thread) {
        modules.market_data_thread->set_allow_fetch_flag(state.allow_fetch);
    }
    if (modules.account_data_thread) {
        modules.account_data_thread->set_allow_fetch_flag(state.allow_fetch);
    }
    if (modules.trading_thread) {
        modules.trading_thread->set_allow_fetch_flag(state.allow_fetch);
    }
}

SystemThreads SystemManager::startup(SystemState& system_state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger) {
    // Create handles for the threads
    SystemThreads handles;
    
    // Initialize the global logging system FIRST
    if (logger) {
        AlpacaTrader::Logging::initialize_global_logger(*logger);
    }
    
    // Create all trading system modules and store them in system_state for lifetime management
    system_state.trading_modules = std::make_unique<SystemModules>(create_trading_modules(system_state, logger, handles));
    
        // Set up data synchronization for trading engine
        DataSyncConfig sync_config(system_state.mtx, system_state.cv, system_state.market, system_state.account,
                                   system_state.has_market, system_state.has_account, system_state.running, system_state.allow_fetch,
                                   system_state.market_data_timestamp, system_state.market_data_fresh, system_state.last_order_timestamp);
        system_state.trading_modules->trading_logic->setup_data_synchronization(sync_config);
        
        // Set up market data fetcher sync state
        AlpacaTrader::Core::MarketDataSyncState fetcher_sync_state = AlpacaTrader::Core::DataSyncReferences(sync_config).to_market_data_sync_state();
        system_state.trading_modules->trading_coordinator->get_market_data_fetcher_reference().set_sync_state_references(fetcher_sync_state);
    
    // Log startup information
    StartupLogs::log_startup_information(*system_state.trading_modules, system_state.config);
    
    // Configure trading modules
    configure_trading_modules(handles, *system_state.trading_modules, system_state);
    
    // Create thread configurations from single source
    auto& modules = *system_state.trading_modules;
    auto thread_definitions = AlpacaTrader::Core::ThreadRegistry::create_thread_definitions(handles, modules, system_state.config);
    auto thread_infos = AlpacaTrader::Core::ThreadRegistry::create_thread_infos(thread_definitions);
    
    // Start all threads
    try {
        if (!system_state.logging_context) {
            throw std::runtime_error("Logging context not initialized - system must fail without context");
        }
        Manager::start_threads(system_state.thread_manager_state, thread_definitions, modules, *system_state.logging_context);
    } catch (const std::exception& exception_error) {
        log_message(std::string("ERROR: Error starting threads: ") + exception_error.what(), "");
        throw;
    }
    
    // Setup thread priorities after threads are started
    try {
        Manager::setup_thread_priorities(system_state.thread_manager_state, thread_definitions, system_state.config);
    } catch (const std::exception& exception_error) {
        log_message(std::string("ERROR: Failed to set thread priorities: ") + exception_error.what(), "");
        throw;
    }
    
    // Store thread infos for monitoring
    system_state.thread_infos = std::move(thread_infos);
    
    return handles;
}

static void run_until_shutdown(SystemState& state) {
    try {
        // Ensure running flag is properly initialized
        if (!state.running.load()) {
            log_message("WARNING: running flag is false at start", "");
            state.running.store(true);
        }
        
        auto start_time = std::chrono::steady_clock::now();
        auto last_monitor_time = start_time;
        
        while (state.running.load()) {
            try {
                auto now = std::chrono::steady_clock::now();

                // Check if thread monitoring is enabled and it's time to log stats (configurable frequency)
                if (state.config.timing.enable_system_health_monitoring &&
                    !state.thread_infos.empty() &&
                    std::chrono::duration_cast<std::chrono::seconds>(now - last_monitor_time).count() >= state.config.timing.system_health_logging_interval_seconds) {

                    try {
                        ThreadLogs::log_thread_monitoring_stats(state.thread_infos, start_time);
                        last_monitor_time = now;
                    } catch (const std::exception& exception_error) {
                        log_message(std::string("ERROR: Error logging thread monitoring stats: ") + exception_error.what(), "");
                    } catch (...) {
                        log_message("ERROR: Unknown error logging thread monitoring stats", "");
                    }
                }

                // Sleep for main loop interval based on configuration
                std::this_thread::sleep_for(std::chrono::seconds(state.config.timing.thread_market_data_poll_interval_sec));
            } catch (const std::exception& exception_error) {
                log_message(std::string("ERROR: Error in main loop: ") + exception_error.what(), "");
            } catch (...) {
                log_message("ERROR: Unknown error in main loop", "");
            }
        }
    } catch (const std::exception& exception_error) {
        log_message(std::string("FATAL: Fatal error in run_until_shutdown: ") + exception_error.what(), "");
        state.running.store(false);
    } catch (...) {
        log_message("FATAL: Unknown fatal error in run_until_shutdown", "");
        state.running.store(false);
    }
}

void SystemManager::shutdown(SystemState& system_state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger) {
    // Signal all threads to stop
    system_state.cv.notify_all();

    // Wait for all threads to complete
    Manager::shutdown_threads(system_state.thread_manager_state);

    // Cleanup API manager - handled automatically by unique_ptr
    if (system_state.trading_modules->api_manager) {
        system_state.trading_modules->api_manager->shutdown();
    }

    // Shutdown logging system
    AlpacaTrader::Logging::shutdown_global_logger(*logger);
}

// Thread management functions moved to thread_registry.hpp/.cpp
// This provides a single source of truth for all thread definitions

void SystemManager::run(SystemState& system_state) {
    run_until_shutdown(system_state);
}