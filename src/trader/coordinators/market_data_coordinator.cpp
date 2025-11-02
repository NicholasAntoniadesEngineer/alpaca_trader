#include "market_data_coordinator.hpp"
#include "trader/market_data/market_bars_manager.hpp"
#include "trader/market_data/market_data_validator.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "configs/system_config.hpp"
#include "logging/logs/market_data_thread_logs.hpp"
#include "logging/logs/market_data_logs.hpp"
#include "logging/logger/logging_macros.hpp"
#include <chrono>

using namespace AlpacaTrader::Logging;

namespace AlpacaTrader {
namespace Core {

MarketDataCoordinator::MarketDataCoordinator(MarketDataManager& market_data_manager_ref)
    : market_data_manager(market_data_manager_ref) {}

ProcessedData MarketDataCoordinator::fetch_and_process_market_data(const std::string& trading_symbol, std::vector<Bar>& historical_bars_output) {
    // Validate symbol matches MarketDataManager's configured symbol
    const SystemConfig& manager_config = market_data_manager.get_config();
    if (!trading_symbol.empty() && trading_symbol != manager_config.strategy.symbol) {
        MarketDataThreadLogs::log_thread_loop_exception("Symbol mismatch: requested " + trading_symbol + " but manager configured for " + manager_config.strategy.symbol);
    }
    
    try {
        // Log market data fetch start
        MarketDataLogs::log_market_data_fetch_table(trading_symbol, manager_config.logging.log_file);
        
        // MarketDataManager fetches bars internally and returns them to avoid duplicate fetching
        auto fetch_result = market_data_manager.fetch_and_process_market_data();
        
        ProcessedData processed_data = fetch_result.first;
        historical_bars_output = fetch_result.second;
        
        // Log position data and warnings
        MarketDataLogs::log_position_data_and_warnings(
            processed_data.pos_details.position_quantity,
            processed_data.pos_details.current_value,
            processed_data.pos_details.unrealized_pl,
            processed_data.exposure_pct,
            processed_data.open_orders,
            manager_config.logging.log_file,
            manager_config.strategy.position_long_string,
            manager_config.strategy.position_short_string
        );
        
        return processed_data;
        
    } catch (const std::runtime_error& runtime_error) {
        MarketDataLogs::log_market_data_failure_summary(
            trading_symbol,
            "Runtime Error",
            runtime_error.what(),
            0,
            manager_config.logging.log_file
        );
        return ProcessedData{};
    } catch (const std::exception& exception_error) {
        MarketDataLogs::log_market_data_failure_summary(
            trading_symbol,
            "Exception",
            std::string("Exception in fetch_and_process_market_data: ") + exception_error.what(),
            0,
            manager_config.logging.log_file
        );
        return ProcessedData{};
    } catch (...) {
        MarketDataLogs::log_market_data_failure_summary(
            trading_symbol,
            "Unknown Error",
            "Unknown exception in fetch_and_process_market_data",
            0,
            manager_config.logging.log_file
        );
        return ProcessedData{};
    }
}

void MarketDataCoordinator::update_shared_market_snapshot(const ProcessedData& processed_data_result, MarketDataSnapshotState& snapshot_state) {
    if (processed_data_result.atr == 0.0) {
        return;
    }
    
    std::lock_guard<std::mutex> state_lock(snapshot_state.state_mutex);
    
    snapshot_state.market_snapshot.atr = processed_data_result.atr;
    snapshot_state.market_snapshot.avg_atr = processed_data_result.avg_atr;
    snapshot_state.market_snapshot.avg_vol = processed_data_result.avg_vol;
    snapshot_state.market_snapshot.curr = processed_data_result.curr;
    snapshot_state.market_snapshot.prev = processed_data_result.prev;
    snapshot_state.has_market_flag.store(true);
    
    auto current_timestamp = std::chrono::steady_clock::now();
    snapshot_state.market_data_timestamp.store(current_timestamp);
    snapshot_state.market_data_fresh_flag.store(true);
    
    snapshot_state.data_condition_variable.notify_all();
}

void MarketDataCoordinator::process_market_data_iteration(const std::string& symbol, MarketDataSnapshotState& snapshot_state, std::chrono::steady_clock::time_point& last_bar_log_time, Bar& previous_bar) {
    try {

        std::vector<Bar> historical_bars_for_logging;
        ProcessedData computed_data = fetch_and_process_market_data(symbol, historical_bars_for_logging);
        
        if (computed_data.atr == 0.0) {
            MarketDataThreadLogs::log_zero_atr_warning(symbol);
            return;
        }
        
        update_shared_market_snapshot(computed_data, snapshot_state);
        
        MarketDataValidator& validator = market_data_manager.get_market_data_validator();
        const SystemConfig& config = market_data_manager.get_config();
        API::ApiManager& api_manager = market_data_manager.get_api_manager();
        MarketDataThreadLogs::process_csv_logging_if_needed(computed_data, historical_bars_for_logging, validator, symbol, config.timing, api_manager, last_bar_log_time, previous_bar);
        
    } catch (const std::exception& exception_error) {
        MarketDataThreadLogs::log_thread_loop_exception("Error in process_market_data_iteration: " + std::string(exception_error.what()));
    } catch (...) {
        MarketDataThreadLogs::log_thread_loop_exception("Unknown error in process_market_data_iteration");
    }
}

} // namespace Core
} // namespace AlpacaTrader

