#include "trading_coordinator.hpp"
#include "logging/logs/trading_logs.hpp"
#include "logging/logger/csv_trade_logger.hpp"
#include "utils/time_utils.hpp"
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

TradingCoordinator::TradingCoordinator(TradingLogic& trading_logic_ref, MarketDataFetcher& data_fetcher_ref,
                                       ConnectivityManager& connectivity_manager_ref,
                                       AccountManager& account_manager_ref,
                                       const SystemConfig& system_config_param)
    : trading_logic(trading_logic_ref), data_fetcher(data_fetcher_ref), 
      connectivity_manager(connectivity_manager_ref), account_manager(account_manager_ref),
      config(system_config_param) {}

void TradingCoordinator::execute_trading_cycle_iteration(TradingSnapshotState& snapshot_state,
                                                          MarketDataSyncState& market_data_sync_state,
                                                          double initial_equity,
                                                          unsigned long loop_counter_value) {
    // Check connectivity status
    if (!connectivity_manager.check_connectivity_status()) {
        std::string connectivity_msg = "Connectivity outage - status: " + connectivity_manager.get_status_string();
        TradingLogs::log_market_status(false, connectivity_msg);
        trading_logic.handle_trading_halt("Connectivity issues detected");
        return;
    }

    // Wait for fresh market data
    data_fetcher.wait_for_fresh_data(market_data_sync_state);
    
    if (!snapshot_state.running_flag.load()) {
        return;
    }

    // Lock and read snapshots atomically
    MarketSnapshot current_market_snapshot;
    AccountSnapshot current_account_snapshot;
    {
        std::lock_guard<std::mutex> state_lock(snapshot_state.state_mutex);
        current_market_snapshot = snapshot_state.market_snapshot;
        current_account_snapshot = snapshot_state.account_snapshot;
    }

    // Validate we have required data
    if (!snapshot_state.has_market_flag.load() || !snapshot_state.has_account_flag.load()) {
        TradingLogs::log_market_status(false, "Missing required snapshot data");
        return;
    }

    // Log loop header
    if (config.trading_mode.primary_symbol.empty()) {
        TradingLogs::log_market_status(false, "Primary symbol is required but not configured");
        return;
    }
    std::string symbol = config.trading_mode.primary_symbol;
    TradingLogs::log_loop_header(loop_counter_value, symbol);

    // CSV logging for account updates
    try {
        std::string timestamp = TimeUtils::get_current_human_readable_time();
        double buying_power = account_manager.fetch_buying_power();
        if (auto csv_trade_logger = AlpacaTrader::Logging::get_csv_trade_logger()) {
            csv_trade_logger->log_account_update(
                timestamp, current_account_snapshot.equity, 
                buying_power, current_account_snapshot.exposure_pct
            );
        }
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_data_result_table("CSV logging error in account update: " + std::string(exception_error.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown CSV logging error in account update", false, 0);
    }

    // Execute trading cycle via engine
    trading_logic.execute_trading_cycle(current_market_snapshot, current_account_snapshot, initial_equity);
}

MarketDataFetcher& TradingCoordinator::get_market_data_fetcher_reference() {
    return data_fetcher;
}

void TradingCoordinator::process_trading_cycle_iteration(MarketSnapshot& market_snapshot,
                                                         AccountSnapshot& account_snapshot,
                                                         std::mutex& state_mtx,
                                                         std::condition_variable& data_cv,
                                                         std::atomic<bool>& has_market,
                                                         std::atomic<bool>& has_account,
                                                         std::atomic<bool>& running,
                                                         std::atomic<std::chrono::steady_clock::time_point>& market_data_timestamp,
                                                         std::atomic<bool>& market_data_fresh,
                                                         std::atomic<std::chrono::steady_clock::time_point>& last_order_timestamp,
                                                         std::atomic<bool>* allow_fetch_ptr,
                                                         double initial_equity,
                                                         std::atomic<unsigned long>& loop_counter) {
    try {
        // Increment loop counter
        unsigned long current_loop_counter = loop_counter.fetch_add(1) + 1;
        
        // Create snapshot state structure
        TradingSnapshotState snapshot_state{
            market_snapshot,
            account_snapshot,
            state_mtx,
            data_cv,
            has_market,
            has_account,
            running
        };
        
        // Create market data sync state structure
        MarketDataSyncState market_data_sync_state(
            &state_mtx,
            &data_cv,
            &market_snapshot,
            &account_snapshot,
            &has_market,
            &has_account,
            &running,
            allow_fetch_ptr ? allow_fetch_ptr : &running,  // Use allow_fetch if available, otherwise running
            &market_data_timestamp,
            &market_data_fresh,
            &last_order_timestamp
        );
        
        // Execute trading cycle iteration through coordinator
        execute_trading_cycle_iteration(snapshot_state, market_data_sync_state, initial_equity, current_loop_counter);
        
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_data_result_table("Exception in process_trading_cycle_iteration: " + std::string(exception_error.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown exception in process_trading_cycle_iteration", false, 0);
    }
}

void TradingCoordinator::countdown_to_next_cycle(std::atomic<bool>& running, int poll_interval_sec, int countdown_refresh_interval_sec) {
    int sleep_secs = poll_interval_sec;
    int countdown_refresh_interval = countdown_refresh_interval_sec;
    
    // If countdown refresh interval is 0 or greater than sleep_secs, just sleep once
    if (countdown_refresh_interval <= 0 || countdown_refresh_interval >= sleep_secs) {
        if (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(sleep_secs));
        }
        return;
    }
    
    // Calculate how many countdown updates we should show
    int num_updates = sleep_secs / countdown_refresh_interval;
    int remaining_secs = sleep_secs;
    
    for (int update_index = 0; update_index < num_updates && running.load(); ++update_index) {
        int display_secs = std::min(remaining_secs, countdown_refresh_interval);
        TradingLogs::log_inline_next_loop(display_secs);
        
        if (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(countdown_refresh_interval));
            remaining_secs -= countdown_refresh_interval;
        }
    }
    
    // Sleep any remaining time without countdown display
    if (remaining_secs > 0 && running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(remaining_secs));
    }
    
    TradingLogs::end_inline_status();
}

} // namespace Core
} // namespace AlpacaTrader
