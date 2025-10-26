#ifndef MARKET_DATA_VALIDATOR_HPP
#define MARKET_DATA_VALIDATOR_HPP

#include "configs/system_config.hpp"
#include "data_structures.hpp"
#include "core/logging/market_data_logs.hpp"

using AlpacaTrader::Config::SystemConfig;

namespace AlpacaTrader {
namespace Core {

class MarketDataValidator {
public:
    MarketDataValidator(const SystemConfig& config);

    // Market data validation methods
    bool validate_market_snapshot(const MarketSnapshot& market_snapshot) const;
    bool validate_account_snapshot(const AccountSnapshot& account_snapshot) const;
    bool validate_processed_data(const ProcessedData& processed_data) const;

private:
    const SystemConfig& config;
    
    // Validation helper methods
    bool validate_price_data(const Bar& bar_data) const;
    bool validate_technical_indicators(const MarketSnapshot& market_snapshot) const;
    bool validate_position_data(const PositionDetails& position_details) const;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MARKET_DATA_VALIDATOR_HPP
