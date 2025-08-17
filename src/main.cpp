// main.cpp
#include "Config.hpp"
#include "core/trader.hpp"
#include "api/alpaca_client.hpp"
#include "workers/market_data_worker.hpp"
#include "workers/account_data_worker.hpp"
#include "data/account_manager.hpp"
#include "ui/account_display.hpp"
#include "utils/async_logger.hpp"
#include "configs/trader_config.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <csignal>

// ========== Orchestration helpers ==========

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

// Removed initialize_config - all config now set in Config.hpp constructor

static void start_logging(AsyncLogger& logger) {
    logger.start();
    set_async_logger(&logger);
}

static void stop_logging(AsyncLogger& logger) { 
    logger.stop(); 
}

struct SharedState {
    std::mutex mtx;
    std::condition_variable cv;
    MarketSnapshot market;
    AccountSnapshot account;
    std::atomic<bool> has_market{false};
    std::atomic<bool> has_account{false};
    std::atomic<bool> running{true};
    std::atomic<bool> allow_fetch{true};
};

static void show_startup_account_status(AccountDisplay& account_display) {
    account_display.display_account_status();
}

static void start_workers(MarketDataWorker& market_worker,
                          AccountDataWorker& account_worker,
                          SharedState& state) {
    market_worker.set_allow_fetch_flag(state.allow_fetch);
    account_worker.set_allow_fetch_flag(state.allow_fetch);
    market_worker.start();
    account_worker.start();
}

static void start_trader(SharedState& state, Trader& trader) {
    trader.attach_shared_state(state.mtx, state.cv, state.market, state.account, 
                               state.has_market, state.has_account, state.running);
    trader.run();
    trader.start_decision_thread();
}

static void run_until_shutdown(SharedState& state) {
    while (state.running.load()) std::this_thread::sleep_for(std::chrono::seconds(1));
}

static void shutdown_all(SharedState& state,
                         MarketDataWorker& market_worker,
                         AccountDataWorker& account_worker,
                         Trader& trader) {
    state.cv.notify_all();
    market_worker.join();
    account_worker.join();
    trader.join_decision_thread();
}

static void run_market_gate(SharedState& state, AlpacaClient& client, const Config& config) {
    set_log_thread_tag("GATE  ");
    while (state.running.load()) {
        bool within = client.is_within_fetch_window();
        bool prev = state.allow_fetch.load();
        state.allow_fetch.store(within);
        
        if (within != prev) {
            log_message(std::string("Market fetch gate ") + (within ? "ENABLED" : "DISABLED") +
                        " (pre/post window applied)", config.logging.log_file);
        }
        
        // Always print a heartbeat for this loop
        log_message(std::string("Market fetch gate CHECK: ") + (within ? "ENABLED" : "DISABLED") +
                    " | interval=" + std::to_string(config.timing.market_open_check_sec) + "s" +
                    " | buffers=" + std::to_string(config.timing.pre_open_buffer_min) + "m/" +
                    std::to_string(config.timing.post_close_buffer_min) + "m",
                    config.logging.log_file);
        std::this_thread::sleep_for(std::chrono::seconds(config.timing.market_open_check_sec));
    }
}

static void initialize_application(const Config& config, AsyncLogger& logger) {
    std::string cfgError;
    if (!validate_config(config, cfgError)) {
        fprintf(stderr, "Config error: %s\n", cfgError.c_str());
        exit(1);
    }
    
    start_logging(logger);
    set_log_thread_tag("MAIN  ");
}

static void create_and_start_components(SharedState& shared, 
                                       Trader& trader,
                                       MarketDataWorker& market_worker, 
                                       AccountDataWorker& account_worker,
                                       AccountDisplay& account_display) {
    // Ensure account summary and configuration are printed before starting other threads
    show_startup_account_status(account_display);
    
    start_workers(market_worker, account_worker, shared);
    start_trader(shared, trader);
}

static void run_main_application_loop(SharedState& shared, AlpacaClient& client, 
                                     const Config& config, Trader& trader,
                                     MarketDataWorker& market_worker, 
                                     AccountDataWorker& account_worker) {
    setup_signal_handlers(shared.running);
    
    // Market gate thread: periodically update allow_fetch based on window
    std::thread market_gate([&]{ run_market_gate(shared, client, config); });
    
    run_until_shutdown(shared);
    shutdown_all(shared, market_worker, account_worker, trader);
    
    if (market_gate.joinable()) market_gate.join();
}

int main() {
    Config config;
    AsyncLogger logger(config.logging.log_file);
    
    // Initialize application
    initialize_application(config, logger);
    
    // Create shared state and components
    SharedState shared;
    AlpacaClient client(config.api, config.session, config.logging, config.target, config.timing);
    
    // Create account management and display
    AccountManager account_manager(config.api, config.logging, config.target);
    AccountDisplay account_display(config.logging, account_manager);
    
    // Create trader with grouped config
    TraderConfig trader_config(config.strategy, config.risk, config.timing, config.flags, 
                               config.ux, config.logging, config.target);
    Trader trader(trader_config, client, account_manager);
    
    // Create workers
    MarketDataWorker market_worker(config.strategy, config.timing, config.target, 
                                   client, shared.mtx, shared.cv, shared.market, shared.has_market, shared.running);
    AccountDataWorker account_worker(config.timing, 
                                     account_manager, shared.mtx, shared.cv, shared.account, shared.has_account, shared.running);
    
    // Start components
    create_and_start_components(shared, trader, market_worker, account_worker, account_display);
    
    // Run main application loop
    run_main_application_loop(shared, client, config, trader, market_worker, account_worker);
    
    // Cleanup
    stop_logging(logger);
    return 0;
}
