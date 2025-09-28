#include "price_manager.hpp"
#include "core/logging/trading_logs.hpp"

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

PriceManager::PriceManager(API::AlpacaClient& client_ref, const TraderConfig& cfg)
    : client(client_ref), config(cfg) {}

double PriceManager::get_real_time_price_with_fallback(double fallback_price) const {
    double current_price = client.get_current_price(config.target.symbol);
    
    if (current_price <= 0.0) {
        current_price = fallback_price;
        TradingLogs::log_data_source_info_table("DELAYED BAR DATA (15-MIN DELAY)", current_price, "FREE PLAN LIMITATION");
    } else {
        TradingLogs::log_data_source_info_table("IEX FREE QUOTE", current_price, "LIMITED SYMBOL COVERAGE");
    }
    
    return current_price;
}

} // namespace Core
} // namespace AlpacaTrader
