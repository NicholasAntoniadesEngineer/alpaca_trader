#include "market_data_coordinator.hpp"
#include "core/trader/market_data/market_bars_manager.hpp"
#include "core/trader/data_structures/data_structures.hpp"
#include "configs/system_config.hpp"

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
    
    std::vector<Bar> historical_bars_data = fetch_historical_bars_data(fetch_request.symbol, fetch_request.bars_to_fetch, fetch_request.atr_calculation_bars);
    historical_bars_output = historical_bars_data;
    
    if (historical_bars_data.empty()) {
        return ProcessedData{};
    }
    
    if (!has_sufficient_data_for_analysis(historical_bars_data, fetch_request.atr_calculation_bars)) {
        return ProcessedData{};
    }
    
    return compute_technical_indicators(historical_bars_data);
}

API::ApiManager& MarketDataCoordinator::get_api_manager_reference() {
    return api_manager;
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

bool MarketDataCoordinator::has_sufficient_data_for_analysis(const std::vector<Bar>& historical_bars_data, int required_bars_count) const {
    return validate_market_data_sufficiency(historical_bars_data, required_bars_count);
}

ProcessedData MarketDataCoordinator::compute_technical_indicators(const std::vector<Bar>& historical_bars_data) {
    MarketBarsManager bars_manager_instance(config, api_manager);
    return bars_manager_instance.compute_processed_data_from_bars(historical_bars_data);
}

std::vector<Bar> MarketDataCoordinator::fetch_historical_bars_data(const std::string& trading_symbol, int bars_to_fetch_count, int atr_calculation_period) {
    MarketBarsManager bars_manager_instance(config, api_manager);
    MarketDataFetchRequest fetch_request(trading_symbol, bars_to_fetch_count, atr_calculation_period);
    return bars_manager_instance.fetch_historical_market_data(fetch_request);
}

bool MarketDataCoordinator::validate_market_data_sufficiency(const std::vector<Bar>& historical_bars_data, int minimum_required_bars) const {
    MarketBarsManager bars_manager_instance(config, api_manager);
    return bars_manager_instance.has_sufficient_bars_for_calculations(historical_bars_data, minimum_required_bars);
}

} // namespace Core
} // namespace AlpacaTrader

