#ifndef DATA_VALIDATOR_HPP
#define DATA_VALIDATOR_HPP

#include "configs/system_config.hpp"
#include "data_structures.hpp"
#include "data_sync_structures.hpp"
#include "core/logging/logs/market_data_logs.hpp"
#include "api/general/api_manager.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

class DataValidator {
public:
    DataValidator(const SystemConfig& config);

    // Market data validation methods
    bool validate_market_data(const MarketSnapshot& market) const;
    bool validate_sync_state_pointers(MarketDataSyncState& sync_state) const;
    bool fetch_and_validate_market_bars(ProcessedData& data, API::ApiManager& api_manager, const std::string& symbol) const;

private:
    const SystemConfig& config;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // DATA_VALIDATOR_HPP
