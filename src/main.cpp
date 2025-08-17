// main.cpp
#include "main.hpp"
#include <thread>
#include <chrono>
#include <csignal>
#include <string>

static std::atomic<bool>* g_running_ptr;

static void setup_signal_handlers(std::atomic<bool>& running) {
    g_running_ptr = &running;
    std::signal(SIGINT, [](int){ g_running_ptr->store(false); });
    std::signal(SIGTERM, [](int){ g_running_ptr->store(false); });
}

// Config validation function moved here from Utils
static bool validate_config(const Config& config, std::string& errorMessage) {
    if (config.logging.log_file.empty()) {
        errorMessage = "Logging path is empty";
        return false;
    }
    if (config.api.api_key.empty() || config.api.api_secret.empty()) {
        errorMessage = "API credentials missing";
        return false;
    }
    if (config.api.base_url.empty() || config.api.data_url.empty()) {
        errorMessage = "API URLs missing";
        return false;
    }
    if (config.target.symbol.empty()) {
        errorMessage = "Symbol is missing";
        return false;
    }
    return true;
}
    
static void show_startup_account_status(AccountDisplay& account_display) {
    account_display.display_account_status();
}

static void run_until_shutdown(SystemState& state) {
    while (state.running.load()) std::this_thread::sleep_for(std::chrono::seconds(1));
}

void initialize_application(const Config& config, AsyncLogger& logger) {
    std::string cfgError;
    if (!validate_config(config, cfgError)) {
        fprintf(stderr, "Config error: %s\n", cfgError.c_str());
        exit(1);
    }
    
    initialize_global_logger(logger);
    set_log_thread_tag("MAIN  ");
}

ComponentConfigBundle build_core_configs(const SystemState& state) 
{
    return ComponentConfigBundle{
        AlpacaClientConfig{state.config.api, state.config.session, state.config.logging, state.config.target, state.config.timing},
        AccountManagerConfig{state.config.api, state.config.logging, state.config.target},
        MarketDataTaskConfig{state.config.strategy, state.config.timing, state.config.target},
        AccountDataTaskConfig{state.config.timing}
    };
}

ComponentInstances build_core_components(SystemState& state, const ComponentConfigBundle& cfgs) 
{
    ComponentInstances core_components;

    core_components.client = std::make_unique<AlpacaClient>(cfgs.client);
    core_components.account_manager = std::make_unique<AccountManager>(cfgs.account_mgr);
    core_components.account_display = std::make_unique<AccountDisplay>(state.config.logging, *core_components.account_manager);
    core_components.trader = std::make_unique<Trader>(state.trader_view, *core_components.client, *core_components.account_manager);
    core_components.market_task = std::make_unique<MarketDataTask>(cfgs.market_task, *core_components.client, state.mtx, state.cv, state.market, state.has_market, state.running);
    core_components.account_task = std::make_unique<AccountDataTask>(cfgs.account_task, *core_components.account_manager, state.mtx, state.cv, state.account, state.has_account, state.running);
    core_components.market_gate_task = std::make_unique<MarketGateTask>(state.config.timing, state.config.logging, state.allow_fetch, state.running,*core_components.client);
    
    return core_components;
}

SystemThreads boot_system(SystemState& system_state, ComponentInstances& system_components) 
{
    show_startup_account_status(*system_components.account_display);

    system_components.trader->attach_shared_state(system_state.mtx, system_state.cv, system_state.market, system_state.account,system_state.has_market, system_state.has_account, system_state.running);
    system_components.trader->run();
    system_components.market_task->set_allow_fetch_flag(system_state.allow_fetch);
    system_components.account_task->set_allow_fetch_flag(system_state.allow_fetch);

    setup_signal_handlers(system_state.running);

    SystemThreads handles;
    handles.market = std::thread(std::ref(*system_components.market_task));
    handles.account = std::thread(std::ref(*system_components.account_task));
    handles.gate = std::thread(std::ref(*system_components.market_gate_task));
    handles.trader = std::thread(&Trader::run_decision_loop, system_components.trader.get());

    return handles;
}

void run_and_shutdown_system(SystemState& system_state, SystemThreads& handles) 
{
    run_until_shutdown(system_state);
    system_state.cv.notify_all();
    if (handles.market.joinable()) handles.market.join();
    if (handles.account.joinable()) handles.account.join();
    if (handles.gate.joinable()) handles.gate.join();
    if (handles.trader.joinable()) handles.trader.join();
}

int main() 
{

    Config initial_config;

    SystemState system_state(initial_config);
    
    AsyncLogger logger(system_state.config.logging.log_file);

    initialize_application(system_state.config, logger);

    ComponentConfigBundle core_configs = build_core_configs(system_state);

    ComponentInstances core_components = build_core_components(system_state, core_configs);

    SystemThreads thread_handles = boot_system(system_state, core_components);

    run_and_shutdown_system(system_state, thread_handles);

    shutdown_global_logger(logger);

    return 0;
}
