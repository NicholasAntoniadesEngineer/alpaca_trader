#include "position_manager.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/trader/data/data_structures.hpp"

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

PositionManager::PositionManager(API::ApiManager& api_mgr, const SystemConfig& cfg)
    : api_manager(api_mgr), config(cfg) {}

void PositionManager::handle_market_close_positions(const ProcessedData& data) {
    // Check if market is approaching close - simplified implementation
    if (api_manager.is_market_open(config.trading_mode.primary_symbol)) {
        // Market is still open, no need to close positions yet
        return;
    }
    
    int current_qty = data.pos_details.qty;
    if (current_qty == 0) {
        return;
    }
    
    int minutes_until_close = 5; // Simplified - would need proper market close time calculation
    if (minutes_until_close > 0) {
        TradingLogs::log_market_close_warning(minutes_until_close);
    }
    
    std::string side = (current_qty > 0) ? SIGNAL_SELL : SIGNAL_BUY;
    TradingLogs::log_market_close_position_closure(current_qty, config.trading_mode.primary_symbol, side);
    
    try {
        api_manager.close_position(config.trading_mode.primary_symbol, current_qty);
        TradingLogs::log_market_status(true, "Market close position closure executed successfully");
    } catch (const std::exception& e) {
        TradingLogs::log_market_status(false, "Market close position closure failed: " + std::string(e.what()));
    }
    
    TradingLogs::log_market_close_complete();
}

} // namespace Core
} // namespace AlpacaTrader
