#ifndef POSITION_MANAGER_HPP
#define POSITION_MANAGER_HPP

#include "configs/system_config.hpp"
#include "core/trader/data/data_structures.hpp"
#include "api/alpaca_client.hpp"
#include "core/logging/trading_logs.hpp"

namespace AlpacaTrader {
namespace Core {

class PositionManager {
public:
    PositionManager(API::AlpacaClient& client, const SystemConfig& config);
    
    void handle_market_close_positions(const ProcessedData& data);
    
private:
    API::AlpacaClient& client;
    const SystemConfig& config;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // POSITION_MANAGER_HPP
