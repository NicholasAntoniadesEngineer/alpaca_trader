#include "trader.hpp"
#include "core/logging/logger/async_logger.hpp"
#include "core/logging/logger/logging_macros.hpp"
#include "core/logging/logger/csv_trade_logger.hpp"
#include "core/threads/thread_logic/platform/thread_control.hpp"
#include "core/utils/time_utils.hpp"
#include "core/trader/data/market_processing.hpp"
#include <thread>
#include <chrono>
#include <cmath>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

TradingOrchestrator::TradingOrchestrator(const TradingOrchestratorConstructionParams& construction_params)
    : config(construction_params.system_config), account_manager(construction_params.account_manager_ref),
      trading_engine(TradingEngineConstructionParams(construction_params.system_config, construction_params.api_manager_ref, construction_params.account_manager_ref, construction_params.system_monitor_ref, construction_params.connectivity_manager_ref)),
      risk_manager(construction_params.system_config),
      data_fetcher(construction_params.api_manager_ref, construction_params.account_manager_ref, construction_params.system_config),
      connectivity_manager(construction_params.connectivity_manager_ref) {
    
    double fetched_initial_equity = initialize_trading_session();
    if (fetched_initial_equity <= 0.0 || !std::isfinite(fetched_initial_equity)) {
        throw std::runtime_error("Failed to initialize trading session - invalid initial equity: " + std::to_string(fetched_initial_equity));
    }
    runtime.initial_equity = fetched_initial_equity;
}


double TradingOrchestrator::initialize_trading_session() {
    return account_manager.fetch_account_equity();
}

void TradingOrchestrator::execute_trading_loop() {
    try {
        while (data_sync.running && data_sync.running->load()) {
            try {
                // Check connectivity status
                if (!connectivity_manager.check_connectivity_status()) {
                    std::string connectivity_msg = "Connectivity outage - status: " + connectivity_manager.get_status_string();
                    TradingLogs::log_market_status(false, connectivity_msg);
                    trading_engine.handle_trading_halt("Connectivity issues detected");
                    countdown_to_next_cycle();
                    continue;
                }

                // Wait for fresh market data to be available and check if we should continue running
                MarketDataSyncState sync_state = data_sync.to_market_data_sync_state();
                data_fetcher.wait_for_fresh_data(sync_state);
                if (!data_sync.running->load()) break;

                // Fetch current market and account data
                auto market_and_account_snapshots = data_fetcher.fetch_current_snapshots();
                MarketSnapshot current_market_snapshot = market_and_account_snapshots.first;
                AccountSnapshot current_account_snapshot = market_and_account_snapshots.second;

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

                    if (auto csv_trade_logger = AlpacaTrader::Logging::get_csv_trade_logger()) {
                        csv_trade_logger->log_account_update(
                            timestamp, current_account_snapshot.equity, buying_power, current_account_snapshot.exposure_pct
                        );
                    }
                } catch (const std::exception& exception_error) {
                    TradingLogs::log_market_data_result_table("CSV logging error in account update: " + std::string(exception_error.what()), false, 0);
                } catch (...) {
                    TradingLogs::log_market_data_result_table("Unknown CSV logging error in account update", false, 0);
                }

                // Validate trading permissions
                if (!risk_manager.validate_trading_permissions(ProcessedData{}, current_account_snapshot.equity, runtime.initial_equity)) {
                    trading_engine.handle_trading_halt("Trading conditions not met");
                    continue;
                }

                // Process trading cycle
                if (!data_fetcher.get_market_data_validator().validate_market_snapshot(current_market_snapshot)) {
                    countdown_to_next_cycle();
                    continue;
                }
                ProcessedData processed_data_for_trading(current_market_snapshot, current_account_snapshot);
                trading_engine.handle_market_close_positions(processed_data_for_trading);
                trading_engine.execute_trading_decision(processed_data_for_trading, current_account_snapshot.equity);

                // Increment iteration counter
                if (runtime.iteration_counter) {
                    runtime.iteration_counter->fetch_add(1);
                }

                countdown_to_next_cycle();

            } catch (const std::exception& exception_error) {
                TradingLogs::log_market_data_result_table("Exception in trading cycle: " + std::string(exception_error.what()), false, 0);
                int recovery_sleep_secs = config.timing.exception_recovery_sleep_seconds;
                std::this_thread::sleep_for(std::chrono::seconds(recovery_sleep_secs));
            } catch (...) {
                TradingLogs::log_market_data_result_table("Unknown exception in trading cycle", false, 0);
                int recovery_sleep_secs = config.timing.exception_recovery_sleep_seconds;
                std::this_thread::sleep_for(std::chrono::seconds(recovery_sleep_secs));
            }
        }
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_data_result_table("Critical exception in trading loop: " + std::string(exception_error.what()), false, 0);
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

    for (int update_index = 0; update_index < num_updates && data_sync.running->load(); ++update_index) {
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

void TradingOrchestrator::setup_data_synchronization(const DataSyncConfig& sync_configuration) {
    
    // Initialize data synchronization references with the provided configuration
    data_sync = DataSyncReferences(sync_configuration);
    
    // Set up MarketDataFetcher with sync state using safe conversion into persistent storage
    fetcher_sync_state = data_sync.to_market_data_sync_state();
    data_fetcher.set_sync_state_references(fetcher_sync_state);
    
    // Validate that all critical synchronization pointers are properly initialized
    if (!(data_sync.mtx && data_sync.cv && data_sync.market && data_sync.account && 
          data_sync.has_market && data_sync.has_account && data_sync.running && data_sync.allow_fetch)) {
        throw std::runtime_error("Invalid data sync configuration: one or more required pointers are null");
    }
}

} // namespace Core
} // namespace AlpacaTrader