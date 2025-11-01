#include "trading_coordinator.hpp"
#include "logging/logs/trading_logs.hpp"
#include "logging/logger/csv_trade_logger.hpp"
#include "utils/time_utils.hpp"
#include <thread>
#include <chrono>
#include <cmath>

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
    if (!validate_connectivity_status()) {
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

TradingLogic& TradingCoordinator::get_trading_logic_reference() {
    return trading_logic;
}

MarketDataFetcher& TradingCoordinator::get_market_data_fetcher_reference() {
    return data_fetcher;
}

bool TradingCoordinator::validate_connectivity_status() const {
    return connectivity_manager.check_connectivity_status();
}


} // namespace Core
} // namespace AlpacaTrader
