#ifndef MARKET_DATA_COORDINATOR_HPP
#define MARKET_DATA_COORDINATOR_HPP

#include "configs/system_config.hpp"
#include "api/general/api_manager.hpp"
#include "trader/data_structures/data_structures.hpp"
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>

namespace AlpacaTrader {
namespace Core {

using Config::SystemConfig;

class MarketDataCoordinator {
public:
    struct MarketDataSnapshotState {
        MarketSnapshot& market_snapshot;
        std::mutex& state_mutex;
        std::condition_variable& data_condition_variable;
        std::atomic<bool>& has_market_flag;
        std::atomic<std::chrono::steady_clock::time_point>& market_data_timestamp;
        std::atomic<bool>& market_data_fresh_flag;
    };

    MarketDataCoordinator(API::ApiManager& api_manager_ref, const SystemConfig& system_config_param);
    
    ProcessedData fetch_and_process_market_data(const std::string& trading_symbol, std::vector<Bar>& historical_bars_output);
    void update_shared_market_snapshot(const ProcessedData& processed_data_result, MarketDataSnapshotState& snapshot_state);
    bool has_sufficient_data_for_analysis(const std::vector<Bar>& historical_bars_data, int required_bars_count) const;
    API::ApiManager& get_api_manager_reference();

private:
    API::ApiManager& api_manager;
    const SystemConfig& config;
    
    ProcessedData compute_technical_indicators(const std::vector<Bar>& historical_bars_data);
    std::vector<Bar> fetch_historical_bars_data(const std::string& trading_symbol, int bars_to_fetch_count, int atr_calculation_period);
    bool validate_market_data_sufficiency(const std::vector<Bar>& historical_bars_data, int minimum_required_bars) const;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MARKET_DATA_COORDINATOR_HPP

