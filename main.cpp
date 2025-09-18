// main.cpp
#include "main.hpp"
#include "threads/thread_manager.hpp"
#include "logging/startup_logger.hpp"

#include <thread>
#include <chrono>
#include <csignal>
#include <string>
#include <cstdlib>
#include "utils/config_loader.hpp"

static std::atomic<bool>* g_running_ptr;

static void setup_signal_handlers(std::atomic<bool>& running) {
    g_running_ptr = &running;
    std::signal(SIGINT, [](int){ g_running_ptr->store(false); });
    std::signal(SIGTERM, [](int){ g_running_ptr->store(false); });
}

// Config validation
static bool validate_config(const SystemConfig& config, std::string& errorMessage) {
    if (config.api.api_key.empty() || config.api.api_secret.empty()) {
        errorMessage = "API credentials missing (provide via CONFIG_CSV)";
        return false;
    }
    if (config.api.base_url.empty() || config.api.data_url.empty()) {
        errorMessage = "API URLs missing (provide via CONFIG_CSV)";
        return false;
    }
    if (config.target.symbol.empty()) {
        errorMessage = "Symbol is missing (provide via CONFIG_CSV)";
        return false;
    }
    if (config.logging.log_file.empty()) {
        errorMessage = "Logging path is empty (provide via CONFIG_CSV)";
        return false;
    }
    if (config.strategy.atr_period < 2) {
        errorMessage = "strategy.atr_period must be >= 2";
        return false;
    }
    if (config.strategy.rr_ratio <= 0.0) {
        errorMessage = "strategy.rr_ratio must be > 0";
        return false;
    }
    if (config.risk.risk_per_trade <= 0.0 || config.risk.risk_per_trade >= 1.0) {
        errorMessage = "risk.risk_per_trade must be between 0 and 1";
        return false;
    }
    if (config.risk.max_exposure_pct < 0.0 || config.risk.max_exposure_pct > 100.0) {
        errorMessage = "risk.max_exposure_pct must be between 0 and 100";
        return false;
    }
    if (config.timing.sleep_interval_sec <= 0 || config.timing.account_poll_sec <= 0) {
        errorMessage = "timing.* seconds must be > 0";
        return false;
    }
    return true;
}
    
static void show_startup_account_status(AccountManager& account_manager) {
    StartupLogger::log_account_status_header();
    StartupLogger::log_account_overview(account_manager);
    StartupLogger::log_financial_summary(account_manager);
    StartupLogger::log_current_positions(account_manager);
    StartupLogger::log_account_status_footer();
}

static void run_until_shutdown(SystemState& state, SystemThreads& handles) {
    auto last_monitoring_time = std::chrono::steady_clock::now();
    const auto monitoring_interval = std::chrono::seconds(state.config.timing.monitoring_interval_sec);
    
    while (state.running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Check if it's time for periodic monitoring
        auto current_time = std::chrono::steady_clock::now();
        if (current_time - last_monitoring_time >= monitoring_interval) {
            ThreadSystem::Manager::log_thread_monitoring_stats(handles);
            last_monitoring_time = current_time;
        }
    }
}

void initialize_application(const SystemConfig& config, AsyncLogger& logger) {
    std::string cfgError;
    if (!validate_config(config, cfgError)) {
        fprintf(stderr, "Config error: %s\n", cfgError.c_str());
        exit(1);
    }
    
    // Initialize global logging system
    initialize_global_logger(logger);
    set_log_thread_tag("MAIN  ");
    
    // Start with ALPACA TRADER header
    StartupLogger::log_application_header();
}

void log_data_source_configuration(const SystemConfig& config) {
    StartupLogger::log_data_source_configuration(config);
}

ComponentConfigBundle build_core_configs(const SystemState& state) {
    return ComponentConfigBundle{
        AlpacaClientConfig{state.config.api, state.config.session, state.config.logging, state.config.target, state.config.timing},
        AccountManagerConfig{state.config.api, state.config.logging, state.config.target},
        MarketDataThreadConfig{state.config.strategy, state.config.timing, state.config.target},
        AccountDataThreadConfig{state.config.timing}
    };
}

ComponentInstances build_core_components(SystemState& state, const ComponentConfigBundle& cfgs) {
    ComponentInstances core_components;
    core_components.client = std::make_unique<AlpacaClient>(cfgs.client);
    core_components.account_manager = std::make_unique<AccountManager>(cfgs.account_mgr);
    core_components.account_display = std::make_unique<AccountDisplay>(state.config.logging, *core_components.account_manager);
    core_components.trader = std::make_unique<Trader>(state.trader_view, *core_components.client, *core_components.account_manager);
    core_components.market_thread = std::make_unique<MarketDataThread>(cfgs.market_thread, *core_components.client, state.mtx, state.cv, state.market, state.has_market, state.running);
    core_components.account_thread = std::make_unique<AccountDataThread>(cfgs.account_thread, *core_components.account_manager, state.mtx, state.cv, state.account, state.has_account, state.running);
    core_components.market_gate_thread = std::make_unique<MarketGateThread>(state.config.timing, state.config.logging, state.allow_fetch, state.running,*core_components.client);
    
    // Thread objects will be created in boot_system with access to thread counters
    core_components.logging_thread = nullptr;  // Will be created in boot_system
    core_components.trader_thread = nullptr;   // Will be created in boot_system
    
    return core_components;
}

SystemThreads boot_system(SystemState& system_state, ComponentInstances& system_components, AsyncLogger& logger) 
{
    show_startup_account_status(*system_components.account_manager);
    log_data_source_configuration(system_state.config);

    system_components.trader->attach_shared_state(system_state.mtx, system_state.cv, system_state.market, system_state.account,system_state.has_market, system_state.has_account, system_state.running);
    system_components.trader->run();
    system_components.market_thread->set_allow_fetch_flag(system_state.allow_fetch);
    system_components.account_thread->set_allow_fetch_flag(system_state.allow_fetch);

    setup_signal_handlers(system_state.running);

    SystemThreads handles;  
    system_components.logging_thread = std::make_unique<LoggingThread>(
        logger.get_file_path(),
        logger.mtx,
        logger.cv,
        logger.queue,
        logger.running,
        handles.logger_iterations
    );
    system_components.trader_thread = std::make_unique<TraderThread>(
        *system_components.trader,
        handles.trader_iterations
    );

    ThreadSystem::Manager::log_thread_startup_info(system_state.config.timing);

    system_components.market_thread->set_iteration_counter(handles.market_iterations);
    system_components.account_thread->set_iteration_counter(handles.account_iterations);
    system_components.market_gate_thread->set_iteration_counter(handles.gate_iterations);

    handles.market = std::thread(std::ref(*system_components.market_thread));
    handles.account = std::thread(std::ref(*system_components.account_thread));
    handles.gate = std::thread(std::ref(*system_components.market_gate_thread));
    handles.trader = std::thread(std::ref(*system_components.trader_thread));
    handles.logger = std::thread(std::ref(*system_components.logging_thread));

    // Give threads time to initialize and log their startup messages
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ThreadSystem::Manager::setup_thread_priorities(handles, system_state.config.timing);

    return handles;
}

void run_and_shutdown_system(SystemState& system_state, SystemThreads& handles) 
{
    run_until_shutdown(system_state, handles);
    system_state.cv.notify_all();
    if (handles.market.joinable()) handles.market.join();
    if (handles.account.joinable()) handles.account.join();
    if (handles.gate.joinable()) handles.gate.join();
    if (handles.trader.joinable()) handles.trader.join();
}

int main() 
{
    SystemConfig initial_config;

    std::string csv_path = std::string("config/runtime_config.csv");
    if (!load_config_from_csv(initial_config, csv_path)) {
        fprintf(stderr, "Failed to load config CSV from %s\n", csv_path.c_str());
        return 1;
    }

    SystemState system_state(initial_config);

    AsyncLogger logger(system_state.config.logging.log_file);
    initialize_application(system_state.config, logger);

    ComponentConfigBundle core_configs = build_core_configs(system_state);
    ComponentInstances core_components = build_core_components(system_state, core_configs);

    SystemThreads thread_handles = boot_system(system_state, core_components, logger);

    run_and_shutdown_system(system_state, thread_handles);

    ThreadSystem::Manager::log_thread_monitoring_stats(thread_handles);
    
    // Shutdown logging system
    shutdown_global_logger(logger);

    return 0;
}
