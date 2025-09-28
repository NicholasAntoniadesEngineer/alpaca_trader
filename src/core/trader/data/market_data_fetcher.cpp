#include "market_data_fetcher.hpp"
#include "core/logging/trading_logs.hpp"
#include <cmath>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

MarketDataFetcher::MarketDataFetcher(API::AlpacaClient& client_ref, AccountManager& account_mgr, const TraderConfig& cfg)
    : client(client_ref), account_manager(account_mgr), config(cfg) {}

ProcessedData MarketDataFetcher::fetch_and_process_data() {
    ProcessedData data;
    TradingLogs::log_market_data_fetch_table(config.target.symbol);

    BarRequest br{config.target.symbol, config.strategy.atr_period + config.timing.bar_buffer};
    auto bars = client.get_recent_bars(br);
    if (bars.size() < static_cast<size_t>(config.strategy.atr_period + 2)) {
        if (bars.size() == 0) {
            TradingLogs::log_market_data_result_table("No market data available", false, 0);
        } else {
            TradingLogs::log_market_data_result_table("Insufficient data for analysis", false, bars.size());
        }
        TradingLogs::log_market_data_result_table("Market data collection failed", false, bars.size());
        return data;
    }

    TradingLogs::log_market_data_attempt_table("Computing indicators");

    data = MarketProcessing::compute_processed_data(bars, config);
    if (data.atr == 0.0) {
        TradingLogs::log_market_data_result_table("Indicator computation failed", false, 0);
        return data;
    }

    TradingLogs::log_market_data_attempt_table("Getting position and account data");
    SymbolRequest sr{config.target.symbol};
    data.pos_details = account_manager.fetch_position_details(sr);
    data.open_orders = account_manager.fetch_open_orders_count(sr);
    double equity = account_manager.fetch_account_equity();
    data.exposure_pct = (equity > 0.0) ? (std::abs(data.pos_details.current_value) / equity) * 100.0 : 0.0;

    TradingLogs::log_current_positions_table(data.pos_details.qty, data.pos_details.current_value, data.pos_details.unrealized_pl, data.exposure_pct, data.open_orders);

    if (data.pos_details.qty != 0 && data.open_orders == 0) {
        TradingLogs::log_market_data_result_table("Missing bracket order warning", true, 0);
    }

    return data;
}

} // namespace Core
} // namespace AlpacaTrader
