#ifndef MARKET_DATA_COORDINATOR_HPP
#define MARKET_DATA_COORDINATOR_HPP

#include "configs/system_config.hpp"
#include "api/general/api_manager.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "trader/market_data/market_data_manager.hpp"
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

    MarketDataCoordinator(MarketDataManager& market_data_manager_ref);
    
    ProcessedData fetch_and_process_market_data(const std::string& trading_symbol, std::vector<Bar>& historical_bars_output);
    void update_shared_market_snapshot(const ProcessedData& processed_data_result, MarketDataSnapshotState& snapshot_state);
    
    // Process a complete market data iteration (fetch, process, update snapshot, CSV logging)
    void process_market_data_iteration(const std::string& symbol, MarketDataSnapshotState& snapshot_state, std::chrono::steady_clock::time_point& last_bar_log_time, Bar& previous_bar);

private:
    MarketDataManager& market_data_manager;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MARKET_DATA_COORDINATOR_HPP

