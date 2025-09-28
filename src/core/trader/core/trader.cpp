#include "trader.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/logging_macros.hpp"
#include "core/threads/thread_logic/platform/thread_control.hpp"
#include <thread>
#include <chrono>
#include <iostream>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;
using AlpacaTrader::Logging::set_log_thread_tag;

TradingOrchestrator::TradingOrchestrator(const TraderConfig& cfg, API::AlpacaClient& client_ref, AccountManager& account_mgr)
    : config(cfg), account_manager(account_mgr),
      trading_engine(cfg, client_ref, account_mgr),
      risk_manager(cfg) {
    
    runtime.initial_equity = initialize_trading_session();
}

TradingOrchestrator::~TradingOrchestrator() {}

double TradingOrchestrator::initialize_trading_session() {
    return account_manager.fetch_account_equity();
}

void TradingOrchestrator::execute_trading_loop() {
    try {
        while (shared.running && shared.running->load()) {
            try {
                wait_for_fresh_data();
                if (!shared.running->load()) break;

                auto snapshots = fetch_current_market_data();
                MarketSnapshot market = snapshots.first;
                AccountSnapshot account = snapshots.second;

                display_trading_loop_header();

                if (!risk_manager.validate_trading_permissions(ProcessedData{}, account.equity)) {
                    handle_trading_halt();
                    continue;
                }

                process_trading_cycle(market, account);

                if (runtime.iteration_counter) {
                    runtime.iteration_counter->fetch_add(1);
                }

                countdown_to_next_cycle();

            } catch (const std::exception& e) {
                TradingLogs::log_market_data_result_table("Exception in trading cycle: " + std::string(e.what()), false, 0);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } catch (...) {
                TradingLogs::log_market_data_result_table("Unknown exception in trading cycle", false, 0);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    } catch (const std::exception& e) {
        TradingLogs::log_market_data_result_table("Critical exception in trading loop: " + std::string(e.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Critical unknown exception in trading loop", false, 0);
    }
}

void TradingOrchestrator::wait_for_fresh_data() {
    try {
        if (!(shared.mtx && shared.cv && shared.has_market && shared.has_account && shared.running)) {
            TradingLogs::log_market_data_result_table("Invalid shared state pointers", false, 0);
            return;
        }
        
        std::unique_lock<std::mutex> lock(*shared.mtx);
        shared.cv->wait_for(lock, std::chrono::seconds(1), [&]{
            return (shared.has_market && shared.has_market->load()) && (shared.has_account && shared.has_account->load());
        });
        
        if (!shared.running->load()) {
            return;
        }
        if (!shared.has_market->load()) {
            return;
        }
        
        shared.has_market->store(false);
        lock.unlock();
        
    } catch (const std::exception& e) {
        TradingLogs::log_market_data_result_table("Exception in wait_for_fresh_data: " + std::string(e.what()), false, 0);
        return;
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown exception in wait_for_fresh_data", false, 0);
        return;
    }
}

std::pair<MarketSnapshot, AccountSnapshot> TradingOrchestrator::fetch_current_market_data() {
    if (!(shared.market && shared.account)) {
        return {MarketSnapshot{}, AccountSnapshot{}};
    }
    MarketSnapshot market = *shared.market;
    AccountSnapshot account = *shared.account;
    return {market, account};
}

void TradingOrchestrator::display_trading_loop_header() {
    runtime.loop_counter.fetch_add(1);
    std::string symbol = config.target.symbol.empty() ? "UNKNOWN" : config.target.symbol;
    
    if (symbol.empty()) {
        symbol = "UNKNOWN";
    }
    
    try {
        TradingLogs::log_loop_header(runtime.loop_counter.load(), symbol);
    } catch (const std::exception& e) {
        std::cout << "Trading loop #" << runtime.loop_counter.load() << " - " << symbol << std::endl;
    } catch (...) {
        std::cout << "Trading loop #" << runtime.loop_counter.load() << " - UNKNOWN" << std::endl;
    }
}

void TradingOrchestrator::handle_trading_halt() {
    trading_engine.handle_trading_halt("Trading conditions not met");
}

void TradingOrchestrator::process_trading_cycle(const MarketSnapshot& market, const AccountSnapshot& account) {
    trading_engine.process_trading_cycle(market, account);
}

void TradingOrchestrator::countdown_to_next_cycle() {
    int sleep_secs = config.timing.thread_trader_poll_interval_sec;
    for (int s = sleep_secs; s > 0 && shared.running->load(); --s) {
        TradingLogs::log_inline_next_loop(s);
        std::this_thread::sleep_for(std::chrono::seconds(config.timing.countdown_tick_sec));
    }
    TradingLogs::end_inline_status();
}

void TradingOrchestrator::attach_shared_state(std::mutex& mtx,
                                             std::condition_variable& cv,
                                             MarketSnapshot& market,
                                             AccountSnapshot& account,
                                             std::atomic<bool>& has_market,
                                             std::atomic<bool>& has_account,
                                             std::atomic<bool>& running_flag,
                                             std::atomic<bool>& allow_fetch_flag) {
    shared.mtx = &mtx;
    shared.cv = &cv;
    shared.market = &market;
    shared.account = &account;
    shared.has_market = &has_market;
    shared.has_account = &has_account;
    shared.running = &running_flag;
    shared.allow_fetch = &allow_fetch_flag;
    if (!(shared.mtx && shared.cv && shared.market && shared.account && shared.has_market && shared.has_account && shared.running && shared.allow_fetch)) {
        TradingLogs::log_market_data_result_table("Invalid shared state configuration", false, 0);
    }
}

void TradingOrchestrator::start_decision_thread() {
    runtime.decision_thread = std::thread([](){ set_log_thread_tag("DECIDE"); });
}

void TradingOrchestrator::join_decision_thread() {
    if (runtime.decision_thread.joinable()) runtime.decision_thread.join();
}

void TradingOrchestrator::set_iteration_counter(std::atomic<unsigned long>& counter) {
    runtime.iteration_counter = &counter;
}

} // namespace Core
} // namespace AlpacaTrader