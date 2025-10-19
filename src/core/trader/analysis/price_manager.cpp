#include "price_manager.hpp"
#include "core/logging/trading_logs.hpp"

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

PriceManager::PriceManager(API::ApiManager& api_mgr, const SystemConfig& cfg)
    : api_manager(api_mgr), config(cfg) {}

double PriceManager::get_real_time_price_with_fallback(double fallback_price) const {
    double current_price = api_manager.get_current_price(config.trading_mode.primary_symbol);
    
    if (current_price <= 0.0) {
        current_price = fallback_price;
        TradingLogs::log_data_source_info_table("FALLBACK BAR DATA", current_price, "PROVIDER UNAVAILABLE");
    } else {
        TradingLogs::log_data_source_info_table("REAL-TIME QUOTE", current_price, "MULTI-PROVIDER FEED");
    }
    
    return current_price;
}

} // namespace Core
} // namespace AlpacaTrader
