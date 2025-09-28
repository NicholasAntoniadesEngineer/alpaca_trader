#include "position_manager.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/trader/data/data_structures.hpp"

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

PositionManager::PositionManager(API::AlpacaClient& client_ref, const TraderConfig& cfg)
    : client(client_ref), config(cfg) {}

void PositionManager::handle_market_close_positions(const ProcessedData& data) {
    if (!client.is_approaching_market_close()) {
        return;
    }
    
    int current_qty = data.pos_details.qty;
    if (current_qty == 0) {
        return;
    }
    
    int minutes_until_close = client.get_minutes_until_market_close();
    if (minutes_until_close > 0) {
        TradingLogs::log_market_close_warning(minutes_until_close);
    }
    
    std::string side = (current_qty > 0) ? SIGNAL_SELL : SIGNAL_BUY;
    TradingLogs::log_market_close_position_closure(current_qty, config.target.symbol, side);
    
    client.close_position(ClosePositionRequest{current_qty});
    
    TradingLogs::log_market_close_complete();
}

} // namespace Core
} // namespace AlpacaTrader
