#include "market_data_coordinator.hpp"
#include "trader/market_data/market_bars_manager.hpp"
#include "trader/market_data/market_data_validator.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "configs/system_config.hpp"
#include "logging/logs/market_data_thread_logs.hpp"
#include "logging/logger/logging_macros.hpp"
#include <chrono>

using namespace AlpacaTrader::Logging;

namespace AlpacaTrader {
namespace Core {

MarketDataCoordinator::MarketDataCoordinator(API::ApiManager& api_manager_ref, const SystemConfig& system_config_param)
    : api_manager(api_manager_ref), config(system_config_param) {}

ProcessedData MarketDataCoordinator::fetch_and_process_market_data(const std::string& trading_symbol, std::vector<Bar>& historical_bars_output) {
    if (trading_symbol.empty()) {
        return ProcessedData{};
    }
    
    int bars_to_fetch_count = config.strategy.bars_to_fetch_for_calculations + config.timing.historical_data_buffer_size;
    int atr_calculation_period = config.strategy.atr_calculation_bars;
    
    if (bars_to_fetch_count <= 0 || atr_calculation_period <= 0) {
        return ProcessedData{};
    }
    
    MarketDataFetchRequest fetch_request(trading_symbol, bars_to_fetch_count, atr_calculation_period);
    
    MarketBarsManager bars_manager_instance(config, api_manager);
    std::vector<Bar> historical_bars_data = bars_manager_instance.fetch_historical_market_data(fetch_request);
    historical_bars_output = historical_bars_data;
    
    if (historical_bars_data.empty()) {
        return ProcessedData{};
    }
    
    if (!bars_manager_instance.has_sufficient_bars_for_calculations(historical_bars_data, fetch_request.atr_calculation_bars)) {
        return ProcessedData{};
    }
    
    return bars_manager_instance.compute_processed_data_from_bars(historical_bars_data);
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
        LOG_THREAD_SECTION_HEADER("MARKET DATA FETCH - " + symbol);
        
        std::vector<Bar> historical_bars_for_logging;
        ProcessedData computed_data = fetch_and_process_market_data(symbol, historical_bars_for_logging);
        
        if (computed_data.atr == 0.0) {
            MarketDataThreadLogs::log_zero_atr_warning(symbol);
            LOG_THREAD_SECTION_FOOTER();
            return;
        }
        
        update_shared_market_snapshot(computed_data, snapshot_state);
        
        MarketDataValidator validator_instance(config);
        MarketDataThreadLogs::process_csv_logging_if_needed(computed_data, historical_bars_for_logging, validator_instance, symbol, config.timing, api_manager, last_bar_log_time, previous_bar);
        
        LOG_THREAD_SECTION_FOOTER();
    } catch (const std::exception& exception_error) {
        MarketDataThreadLogs::log_thread_loop_exception("Error in process_market_data_iteration: " + std::string(exception_error.what()));
    } catch (...) {
        MarketDataThreadLogs::log_thread_loop_exception("Unknown error in process_market_data_iteration");
    }
}

} // namespace Core
} // namespace AlpacaTrader

