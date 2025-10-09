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

TradingOrchestrator::TradingOrchestrator(const SystemConfig& cfg, API::AlpacaClient& client_ref, AccountManager& account_mgr)
    : config(cfg), account_manager(account_mgr),
      trading_engine(cfg, client_ref, account_mgr),
      risk_manager(cfg),
      data_fetcher(client_ref, account_mgr, cfg) {
    
    runtime.initial_equity = initialize_trading_session();
}

TradingOrchestrator::~TradingOrchestrator() {}

double TradingOrchestrator::initialize_trading_session() {
    return account_manager.fetch_account_equity();
}

void TradingOrchestrator::execute_trading_loop() {
    try {
        while (data_sync.running && data_sync.running->load()) {
            try {
                // Check connectivity status
                if (!check_connectivity_status()) {
                    trading_engine.handle_trading_halt("Connectivity issues detected");
                    countdown_to_next_cycle();
                    continue;
                }

                // Wait for fresh market data to be available and check if we should continue running
                data_fetcher.wait_for_fresh_data(*reinterpret_cast<MarketDataSyncState*>(&data_sync));
                if (!data_sync.running->load()) break;

                // Fetch current market and account data
                auto snapshots = data_fetcher.fetch_current_snapshots();
                MarketSnapshot market = snapshots.first;
                AccountSnapshot account = snapshots.second;

                // Display trading loop header and increment counter
                runtime.loop_counter.fetch_add(1);
                std::string symbol = config.strategy.symbol.empty() ? "UNKNOWN" : config.strategy.symbol;
                TradingLogs::log_loop_header(runtime.loop_counter.load(), symbol);

                // Validate trading permissions
                if (!risk_manager.validate_trading_permissions(ProcessedData{}, account.equity)) {
                    trading_engine.handle_trading_halt("Trading conditions not met");
                    continue;
                }

                // Process trading cycle
                if (!trading_engine.validate_market_data(market)) {
                    countdown_to_next_cycle();
                    continue;
                }
                ProcessedData processed_data(market, account);
                trading_engine.handle_market_close_positions(processed_data);
                trading_engine.execute_trading_decision(processed_data, account.equity);

                // Increment iteration counter
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

void TradingOrchestrator::countdown_to_next_cycle() {
    int sleep_secs = config.timing.thread_trader_poll_interval_sec;
    
    for (int s = sleep_secs; s > 0 && data_sync.running->load(); --s) {
        TradingLogs::log_inline_next_loop(s);
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Always sleep 1 second between countdown displays
    }
    TradingLogs::end_inline_status();
}

void TradingOrchestrator::setup_data_synchronization(const DataSyncConfig& config) {
    
    // Initialize data synchronization references with the provided configuration
    data_sync = DataSyncReferences(config);
    
    // Set up MarketDataFetcher with sync state
    data_fetcher.set_sync_state_references(*reinterpret_cast<MarketDataSyncState*>(&data_sync));
    
    // Set up TradingEngine with data synchronization
    trading_engine.setup_data_synchronization(config);
    
    if (!(data_sync.mtx && data_sync.cv && data_sync.market && data_sync.account && data_sync.has_market && data_sync.has_account && data_sync.running && data_sync.allow_fetch)) {
        TradingLogs::log_market_data_result_table("Invalid data sync configuration", false, 0);
    }
}

bool TradingOrchestrator::check_connectivity_status() {
    auto& connectivity = ConnectivityManager::instance();
    if (connectivity.is_connectivity_outage()) {
        std::string connectivity_msg = "Connectivity outage - status: " + connectivity.get_status_string();
        TradingLogs::log_market_status(false, connectivity_msg);
        return false;
    }
    return true;
}

} // namespace Core
} // namespace AlpacaTrader