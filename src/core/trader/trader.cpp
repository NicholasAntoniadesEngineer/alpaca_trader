#include "trader.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/logging_macros.hpp"
#include "core/logging/csv_trade_logger.hpp"
#include "core/threads/thread_logic/platform/thread_control.hpp"
#include "core/utils/time_utils.hpp"
#include "core/trader/data/market_processing.hpp"
#include <thread>
#include <chrono>
#include <iostream>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;
using AlpacaTrader::Logging::set_log_thread_tag;

TradingOrchestrator::TradingOrchestrator(const SystemConfig& cfg, API::ApiManager& api_mgr, AccountManager& account_mgr)
    : config(cfg), account_manager(account_mgr),
      trading_engine(cfg, api_mgr, account_mgr),
      risk_manager(cfg),
      data_fetcher(api_mgr, account_mgr, cfg) {
    
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
                if (!ConnectivityManager::instance().check_connectivity_status()) {
                    std::string connectivity_msg = "Connectivity outage - status: " + ConnectivityManager::instance().get_status_string();
                    TradingLogs::log_market_status(false, connectivity_msg);
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
                if (config.trading_mode.primary_symbol.empty()) {
                    throw std::runtime_error("Primary symbol is required but not configured");
                }
                std::string symbol = config.trading_mode.primary_symbol;
                TradingLogs::log_loop_header(runtime.loop_counter.load(), symbol);

                // CSV logging for account updates
                try {
                    std::string timestamp = TimeUtils::get_current_human_readable_time();
                    double buying_power = account_manager.fetch_buying_power();

                    if (AlpacaTrader::Logging::g_csv_trade_logger) {
                        AlpacaTrader::Logging::g_csv_trade_logger->log_account_update(
                            timestamp, account.equity, buying_power, account.exposure_pct
                        );
                    }
                } catch (const std::exception& e) {
                    TradingLogs::log_market_data_result_table("CSV logging error in account update: " + std::string(e.what()), false, 0);
                } catch (...) {
                    TradingLogs::log_market_data_result_table("Unknown CSV logging error in account update", false, 0);
                }

                // Validate trading permissions
                if (!risk_manager.validate_trading_permissions(ProcessedData{}, account.equity)) {
                    trading_engine.handle_trading_halt("Trading conditions not met");
                    continue;
                }

                // Process trading cycle
                if (!data_fetcher.get_market_data_validator().validate_market_snapshot(market)) {
                    countdown_to_next_cycle();
                    continue;
                }
                ProcessedData processed_data(market, account);
                trading_engine.get_order_engine().handle_market_close_positions(processed_data);
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
    int countdown_refresh_interval = config.timing.countdown_display_refresh_interval_seconds;

    // If countdown refresh interval is 0 or greater than sleep_secs, just sleep once
    if (countdown_refresh_interval <= 0 || countdown_refresh_interval >= sleep_secs) {
        if (data_sync.running->load()) {
            std::this_thread::sleep_for(std::chrono::seconds(sleep_secs));
        }
        return;
    }

    // Calculate how many countdown updates we should show
    int num_updates = sleep_secs / countdown_refresh_interval;
    int remaining_secs = sleep_secs;

    for (int i = 0; i < num_updates && data_sync.running->load(); ++i) {
        int display_secs = std::min(remaining_secs, countdown_refresh_interval);
        TradingLogs::log_inline_next_loop(display_secs);

        if (data_sync.running->load()) {
            std::this_thread::sleep_for(std::chrono::seconds(countdown_refresh_interval));
            remaining_secs -= countdown_refresh_interval;
        }
    }

    // Sleep any remaining time without countdown display
    if (remaining_secs > 0 && data_sync.running->load()) {
        std::this_thread::sleep_for(std::chrono::seconds(remaining_secs));
    }

    TradingLogs::end_inline_status();
}

void TradingOrchestrator::setup_data_synchronization(const DataSyncConfig& config) {
    
    // Initialize data synchronization references with the provided configuration
    data_sync = DataSyncReferences(config);
    
    // Set up MarketDataFetcher with sync state
    data_fetcher.set_sync_state_references(*reinterpret_cast<MarketDataSyncState*>(&data_sync));
    
    if (!(data_sync.mtx && data_sync.cv && data_sync.market && data_sync.account && data_sync.has_market && data_sync.has_account && data_sync.running && data_sync.allow_fetch)) {
        TradingLogs::log_market_data_result_table("Invalid data sync configuration", false, 0);
    }
}

} // namespace Core
} // namespace AlpacaTrader