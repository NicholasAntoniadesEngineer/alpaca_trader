#include "market_session_manager.hpp"
#include "core/logging/trading_logs.hpp"

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

MarketSessionManager::MarketSessionManager(const SystemConfig& cfg, API::ApiManager& api_mgr)
    : config(cfg), api_manager(api_mgr) {}

bool MarketSessionManager::is_market_open() const {
    if (!api_manager.is_within_trading_hours(config.trading_mode.primary_symbol)) {
        TradingLogs::log_market_status(false, "Market is closed - outside trading hours");
        return false;
    }
    TradingLogs::log_market_status(true, "Market is open - trading allowed");
    return true;
}

} // namespace Core
} // namespace AlpacaTrader
