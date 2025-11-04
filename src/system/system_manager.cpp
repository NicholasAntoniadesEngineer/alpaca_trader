#include "system_manager.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include "system/system_configurations.hpp"
#include "system/system_modules.hpp"
#include "system/system_monitor.hpp"
#include "system/system_state.hpp"
#include "system/system_threads.hpp"
#include "configs/system_config.hpp"
#include "api/general/api_manager.hpp"
#include "threads/system_threads/account_data_thread.hpp"
#include "threads/system_threads/logging_thread.hpp"
#include "threads/system_threads/market_data_thread.hpp"
#include "threads/system_threads/market_gate_thread.hpp"
#include "threads/system_threads/trader_thread.hpp"
#include "threads/thread_logic/thread_manager.hpp"
#include "threads/thread_logic/thread_registry.hpp"
#include "threads/thread_register.hpp"
#include "logging/logs/account_logs.hpp"
#include "logging/logs/startup_logs.hpp"
#include "logging/logs/system_logs.hpp"
#include "logging/logs/thread_logs.hpp"
#include "logging/logger/async_logger.hpp"
#include "trader/account_management/account_manager.hpp"
#include "trader/coordinators/trading_coordinator.hpp"
#include "trader/config_loader/config_loader.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "trader/market_data/market_data_fetcher.hpp"
#include "trader/strategy_analysis/risk_manager.hpp"
#include "trader/trading_logic/trading_logic.hpp"
#include "trader/trading_logic/trading_logic_structures.hpp"

using namespace AlpacaTrader::Logging;
using namespace AlpacaTrader::Threads;

namespace AlpacaTrader {
namespace System {

SystemInitializationResult initialize() {
    SystemInitializationResult initialization_result;
    
    try {
        // Initialize minimal logging context early - required before any logging calls
        auto early_logging_context = std::make_shared<AlpacaTrader::Logging::LoggingContext>();
        AlpacaTrader::Logging::set_logging_context(*early_logging_context);
        
        // Load system configuration (may call log_message during loading)
        AlpacaTrader::Config::SystemConfig initial_config;
        int config_load_result = load_system_config(initial_config);
        if (config_load_result != 0) {
            SystemLogs::log_fatal_error(std::string("Config load failed with result: ") + std::to_string(config_load_result));
            throw std::runtime_error("System initialization failed: configuration loading failed");
        }
        
        // Create system state
        initialization_result.system_state = std::make_unique<SystemState>(initial_config);
        
        // Transfer logging context to system state
        initialization_result.system_state->logging_context = early_logging_context;
        
        // Initialize application foundation (logging, validation)
        initialization_result.logger = AlpacaTrader::Logging::initialize_application_foundation(initialization_result.system_state->config);
        
        // Initialize CSV logging for bars and trades (stored in logging context)
        auto bars_logger_instance = AlpacaTrader::Logging::initialize_csv_bars_logger("bars_logs");
        auto trade_logger_instance = AlpacaTrader::Logging::initialize_csv_trade_logger("trade_logs");
        
        // Verify initialization succeeded - fail hard if either logger is invalid
        if (!bars_logger_instance || !trade_logger_instance) {
            SystemLogs::log_fatal_error("CSV logger initialization failed: bars or trade logger returned null pointer");
            throw std::runtime_error("System initialization failed: CSV logger initialization failed");
        }
        
    } catch (const std::exception& exception_error) {
        SystemLogs::log_fatal_error(std::string("System initialization exception: ") + exception_error.what());
        throw;
    } catch (...) {
        SystemLogs::log_fatal_error("System initialization unknown exception");
        throw std::runtime_error("System initialization failed: unknown error");
    }
    
    return initialization_result;
}

SystemConfigurations create_trading_configurations(const SystemState& state) {
    return SystemConfigurations{
        AccountDataThreadConfig{state.config.logging, state.config.timing, state.config.strategy}, // AccountManager uses AccountDataThreadConfig
        MarketDataThreadConfig{state.config.strategy, state.config.timing},
        AccountDataThreadConfig{state.config.logging, state.config.timing, state.config.strategy}, // AccountDataThread uses AccountDataThreadConfig
        LoggingThreadConfig{state.config.logging, state.config.timing},
        TraderThreadConfig{state.config.timing}
    };
}

SystemModules create_trading_modules(SystemState& state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger, SystemThreads& thread_handles) {
    SystemConfigurations configs = create_trading_configurations(state);
    SystemModules modules;

    // Create API manager 
    modules.api_manager = std::make_unique<AlpacaTrader::API::ApiManager>(state.config.multi_api, state.connectivity_manager);
    
    // Create account manager (uses AccountDataThreadConfig to match thread config naming pattern)
    AlpacaTrader::Config::AccountDataThreadConfig account_config{
        state.config.logging, state.config.timing, state.config.strategy
    };
    modules.portfolio_manager = std::make_unique<AlpacaTrader::Core::AccountManager>(account_config, *modules.api_manager);
    
    // Create trading logic (creates its own MarketDataManager internally)
    TradingLogicConstructionParams trading_logic_params(state.config, *modules.api_manager, *modules.portfolio_manager, state.connectivity_manager);
    modules.trading_logic = std::make_unique<AlpacaTrader::Core::TradingLogic>(trading_logic_params);
    
    // Create trading coordinator - uses MarketDataManager from TradingLogic
    modules.trading_coordinator = std::make_unique<AlpacaTrader::Core::TradingCoordinator>(
        *modules.trading_logic,
        modules.trading_logic->get_market_data_manager_reference(),
        state.connectivity_manager,
        *modules.portfolio_manager,
        state.config
    );
    
    // Create coordinator interfaces for thread access to trader components
    // MarketDataCoordinator uses MarketDataManager from TradingLogic for detailed logging
    modules.market_data_coordinator = std::make_unique<AlpacaTrader::Core::MarketDataCoordinator>(
        modules.trading_logic->get_market_data_manager_reference()
    );
    modules.account_data_coordinator = std::make_unique<AlpacaTrader::Core::AccountDataCoordinator>(*modules.portfolio_manager);
    modules.market_gate_coordinator = std::make_unique<AlpacaTrader::Core::MarketGateCoordinator>(*modules.api_manager, state.connectivity_manager);
    
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
                                                                   state.allow_fetch, state.running, *modules.market_gate_coordinator, state.config.trading_mode.primary_symbol);
    
    // Create LOGGING thread (uses LoggingThreadConfig from SystemConfigurations)
    modules.logging_thread = std::make_unique<AlpacaTrader::Threads::LoggingThread>(logger, thread_handles.logger_iterations, state.config);
    
    // Get initial equity for trader thread
    double initial_equity = modules.portfolio_manager->fetch_account_equity();
    if (initial_equity <= 0.0 || !std::isfinite(initial_equity)) {
        throw std::runtime_error("Failed to get initial equity for trader thread");
    }
    
    // Create TRADER_DECISION thread (uses TraderThreadConfig from SystemConfigurations)
    modules.trading_thread = std::make_unique<AlpacaTrader::Threads::TraderThread>(
        configs.trader_thread.timing,
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
    try {
        bool counters_configured = AlpacaTrader::Core::ThreadRegistry::configure_thread_iteration_counters(handles, modules);
        if (!counters_configured) {
            SystemLogs::log_system_startup_error("Failed to configure thread iteration counters");
            throw std::runtime_error("Thread iteration counter configuration failed");
        }
    } catch (const std::runtime_error& runtime_error) {
        SystemLogs::log_system_startup_error(std::string("Exception configuring thread iteration counters: ") + runtime_error.what());
        throw;
    } catch (const std::exception& exception_error) {
        SystemLogs::log_system_startup_error(std::string("Exception configuring thread iteration counters: ") + exception_error.what());
        throw;
    } catch (...) {
        SystemLogs::log_system_startup_error("Unknown exception configuring thread iteration counters");
        throw;
    }
    
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

SystemThreads startup(SystemState& system_state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger) {
    // Configure system monitor
    bool config_set_result = system_state.system_monitor.set_configuration(system_state.config);
    if (!config_set_result) {
        SystemLogs::log_system_startup_error("Failed to set system monitor configuration");
        throw std::runtime_error("Failed to set system monitor configuration");
    }
    
    bool config_validated_result = system_state.system_monitor.record_configuration_validated(true);
    if (!config_validated_result) {
        SystemLogs::log_system_startup_error("Failed to record configuration validation");
        throw std::runtime_error("Failed to record configuration validation");
    }
    SystemLogs::log_configuration_validated(true);
    
    // Create handles for the threads
    SystemThreads handles;
    
    // Initialize the global logging system first
    if (!logger) {
        throw std::runtime_error("System startup failed: Logger is required but not provided");
    }
    AlpacaTrader::Logging::initialize_global_logger(*logger);
    
    // Create all trading system modules and store them in system_state for lifetime management
    system_state.trading_modules = std::make_unique<SystemModules>(create_trading_modules(system_state, logger, handles));
    
    // Set up data synchronization for trading engine
    DataSyncConfig sync_config(system_state.mtx, system_state.cv, system_state.market, system_state.account, 
                                system_state.has_market, system_state.has_account, system_state.running, system_state.allow_fetch,
                                system_state.market_data_timestamp, system_state.market_data_fresh, system_state.last_order_timestamp);
    system_state.trading_modules->trading_logic->setup_data_synchronization(sync_config);
    
    // Set up market data manager sync state
    // CRITICAL: Store MarketDataSyncState in a persistent location to avoid use-after-free
    // Create a persistent sync state object that lives in SystemState
    static AlpacaTrader::Core::MarketDataSyncState fetcher_sync_state_persistent = AlpacaTrader::Core::DataSyncReferences(sync_config).to_market_data_sync_state();
    system_state.fetcher_sync_state_ptr = &fetcher_sync_state_persistent;
    
    bool sync_state_set_result = system_state.trading_modules->trading_coordinator->get_market_data_manager_reference().set_sync_state_references(fetcher_sync_state_persistent);
    if (!sync_state_set_result) {
        SystemLogs::log_fatal_error("Failed to set market data manager sync state references");
        throw std::runtime_error("Failed to set market data manager sync state references");
    }

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
            SystemLogs::log_logging_context_error();
            throw std::runtime_error("Logging context not initialized - system must fail without context");
        }
        bool threads_started = Manager::start_threads(system_state.thread_manager_state, thread_definitions, modules, *system_state.logging_context);
        if (!threads_started) {
            SystemLogs::log_thread_startup_error("Failed to start threads");
            throw std::runtime_error("Thread startup failed");
        }
    } catch (const std::exception& exception_error) {
        SystemLogs::log_thread_startup_error(exception_error.what());
        throw;
    } catch (...) {
        SystemLogs::log_thread_startup_error("Unknown error starting threads");
        throw;
    }
    
    // Setup thread priorities after threads are started
    try {
        bool priorities_setup = Manager::setup_thread_priorities(system_state.thread_manager_state, thread_definitions, system_state.config);
        if (!priorities_setup) {
            SystemLogs::log_thread_priority_error("Failed to setup thread priorities");
            throw std::runtime_error("Thread priority setup failed");
        }
    } catch (const std::exception& exception_error) {
        SystemLogs::log_thread_priority_error(exception_error.what());
        throw;
    } catch (...) {
        SystemLogs::log_thread_priority_error("Unknown error setting thread priorities");
        throw;
    }
    
    // Store thread infos for monitoring
    system_state.thread_infos = std::move(thread_infos);
    
    // Record thread startup in system monitor
    int expected_thread_count = static_cast<int>(thread_definitions.size());
    int actual_thread_count = static_cast<int>(system_state.thread_infos.size());
    bool threads_recorded_result = system_state.system_monitor.record_threads_started(expected_thread_count, actual_thread_count);
    if (!threads_recorded_result) {
        SystemLogs::log_system_startup_error("Failed to record threads started in system monitor");
    } else {
        SystemLogs::log_threads_started(expected_thread_count, actual_thread_count);
    }
    
    // Record system startup complete
    bool startup_recorded_result = system_state.system_monitor.record_startup_complete();
    if (!startup_recorded_result) {
        SystemLogs::log_system_startup_error("Failed to record startup complete in system monitor");
    } else {
        SystemLogs::log_startup_complete();
    }
    
    // Log initial health report after all threads have started
    try {
        AlpacaTrader::Monitoring::SystemHealthReport health_report_data = system_state.system_monitor.get_health_report_data();
        SystemLogs::log_health_report_table(
            health_report_data.system_healthy_value,
            health_report_data.startup_complete_value,
            health_report_data.configuration_valid_value,
            health_report_data.all_threads_started_value,
            health_report_data.active_thread_count_value,
            health_report_data.connectivity_issues_count_value,
            health_report_data.critical_errors_count_value,
            health_report_data.uptime_seconds_value
        );
    } catch (const std::exception& exception_error) {
        SystemLogs::log_critical_error(std::string("Error logging initial health report: ") + exception_error.what());
    } catch (...) {
        SystemLogs::log_critical_error("Unknown error logging initial health report");
    }
    
    return handles;
}

static void run_until_shutdown(SystemState& state) {
    try {
        // Ensure running flag is properly initialized
        if (!state.running.load()) {
            SystemLogs::log_running_flag_warning();
            state.running.store(true);
        }
        
        auto start_time = std::chrono::steady_clock::now();
        auto last_monitor_time = start_time;
        
        while (state.running.load() && !state.shutdown_requested.load()) {
            try {
                auto now = std::chrono::steady_clock::now();

                // Check if thread monitoring is enabled and it's time to log stats (configurable frequency)
                if (state.config.timing.enable_system_health_monitoring &&
                    !state.thread_infos.empty() &&
                    std::chrono::duration_cast<std::chrono::seconds>(now - last_monitor_time).count() >= state.config.timing.system_health_logging_interval_seconds) {

                    try {
                        ThreadLogs::log_thread_monitoring_stats(state.thread_infos, start_time);
                        
                        // Update system monitor with current thread health
                        int active_thread_count = static_cast<int>(state.thread_infos.size());
                        bool health_check_recorded = state.system_monitor.record_thread_health_check(active_thread_count);
                        if (!health_check_recorded) {
                            SystemLogs::log_thread_monitoring_error("Failed to record thread health check");
                        }
                        
                        // Check system health and alert if needed
                        if (state.system_monitor.should_alert()) {
                            AlpacaTrader::Monitoring::SystemHealthReport health_report_data = state.system_monitor.get_health_report_data();
                            std::string health_report_formatted_string = SystemLogs::format_health_report_string(
                                health_report_data.system_healthy_value,
                                health_report_data.startup_complete_value,
                                health_report_data.configuration_valid_value,
                                health_report_data.all_threads_started_value,
                                health_report_data.active_thread_count_value,
                                health_report_data.connectivity_issues_count_value,
                                health_report_data.critical_errors_count_value,
                                health_report_data.uptime_seconds_value
                            );
                            SystemLogs::log_system_alert(health_report_formatted_string);
                        }
                        
                        last_monitor_time = now;
                    } catch (const std::exception& exception_error) {
                        SystemLogs::log_thread_monitoring_error(exception_error.what());
                        std::string error_description_string = std::string("Error in thread monitoring: ") + exception_error.what();
                        bool critical_error_recorded = state.system_monitor.record_critical_error(error_description_string);
                        SystemLogs::log_critical_error(error_description_string);
                        if (!critical_error_recorded) {
                            SystemLogs::log_system_warning("Failed to record critical error in system monitor");
                        }
                    } catch (...) {
                        SystemLogs::log_thread_monitoring_error("Unknown error logging thread monitoring stats");
                        std::string unknown_error_description_string = "Unknown error in thread monitoring";
                        bool critical_error_recorded = state.system_monitor.record_critical_error(unknown_error_description_string);
                        SystemLogs::log_critical_error(unknown_error_description_string);
                        if (!critical_error_recorded) {
                            SystemLogs::log_system_warning("Failed to record critical error in system monitor");
                        }
                    }
                }

                // Sleep for main loop interval based on configuration
                std::this_thread::sleep_for(std::chrono::seconds(state.config.timing.thread_market_data_poll_interval_sec));
            } catch (const std::exception& exception_error) {
                SystemLogs::log_main_loop_error(exception_error.what());
            } catch (...) {
                SystemLogs::log_main_loop_error("Unknown error in main loop");
            }
        }
    } catch (const std::exception& exception_error) {
        SystemLogs::log_fatal_error(exception_error.what());
        state.running.store(false);
    } catch (...) {
        SystemLogs::log_fatal_error("Unknown fatal error in run_until_shutdown");
        state.running.store(false);
    }
}

void shutdown(SystemState& system_state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger) {
    try {
    // Signal all threads to stop
        system_state.running.store(false);
        system_state.shutdown_requested.store(true);
    system_state.cv.notify_all();

    // Wait for all threads to complete
    bool threads_shutdown = Manager::shutdown_threads(system_state.thread_manager_state);
    if (!threads_shutdown) {
        SystemLogs::log_system_shutdown_error("Failed to shutdown threads");
    }

    // Cleanup API manager - handled automatically by unique_ptr
    if (system_state.trading_modules->api_manager) {
        system_state.trading_modules->api_manager->shutdown();
    }

    // Shutdown logging system
    AlpacaTrader::Logging::shutdown_global_logger(*logger);
    } catch (const std::exception& shutdownExceptionError) {
        SystemLogs::log_system_shutdown_error("Exception in shutdown: " + std::string(shutdownExceptionError.what()));
    } catch (...) {
        SystemLogs::log_system_shutdown_error("Unknown exception in shutdown");
    }
}

void run(SystemState& system_state) {
    run_until_shutdown(system_state);
}

} // namespace System
} // namespace AlpacaTrader