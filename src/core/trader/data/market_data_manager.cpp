#include "market_data_manager.hpp"
#include "core/logging/market_data_logs.hpp"
#include "core/trader/analysis/indicators.hpp"
#include <cmath>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::MarketDataLogs;

MarketDataManager::MarketDataManager(const SystemConfig& cfg, API::ApiManager& api_mgr, AccountManager& account_mgr)
    : config(cfg), api_manager(api_mgr), account_manager(account_mgr) {}

ProcessedData MarketDataManager::fetch_and_process_market_data() {
    ProcessedData processed_data;
    MarketDataLogs::log_market_data_fetch_table(config.strategy.symbol, config.logging.log_file);

    // Fetch current snapshots
    auto snapshots = fetch_current_snapshots();
    MarketSnapshot market_snapshot = snapshots.first;
    AccountSnapshot account_snapshot = snapshots.second;

    // Create processed data from snapshots
    processed_data = ProcessedData(market_snapshot, account_snapshot);

    // Process account and position data
    process_account_and_position_data(processed_data);

    // Log current positions and check for warnings
    MarketDataLogs::log_position_data_and_warnings(
        processed_data.pos_details.qty,
        processed_data.pos_details.current_value,
        processed_data.pos_details.unrealized_pl,
        processed_data.exposure_pct,
        processed_data.open_orders,
        config.logging.log_file
    );

    return processed_data;
}

std::pair<MarketSnapshot, AccountSnapshot> MarketDataManager::fetch_current_snapshots() {
    MarketSnapshot market_snapshot;
    AccountSnapshot account_snapshot;

    // Fetch bars data for market snapshot
    BarRequest bar_request(config.strategy.symbol, config.strategy.bars_to_fetch_for_calculations);
    std::vector<Bar> bars_data = api_manager.get_recent_bars(bar_request);

    if (!bars_data.empty()) {
        market_snapshot = create_market_snapshot_from_bars(bars_data);
    }

    // Create account snapshot
    account_snapshot = create_account_snapshot();

    return {market_snapshot, account_snapshot};
}

void MarketDataManager::process_account_and_position_data(ProcessedData& processed_data) const {
    MarketDataLogs::log_market_data_attempt_table("Getting position and account data", config.logging.log_file);

    SymbolRequest symbol_request{config.strategy.symbol};
    processed_data.pos_details = account_manager.fetch_position_details(symbol_request);
    processed_data.open_orders = account_manager.fetch_open_orders_count(symbol_request);

    double account_equity = account_manager.fetch_account_equity();
    if (account_equity <= 0.0) {
        processed_data.exposure_pct = 0.0;
    } else {
        processed_data.exposure_pct = (std::abs(processed_data.pos_details.current_value) / account_equity) * 100.0;
    }
}

MarketSnapshot MarketDataManager::create_market_snapshot_from_bars(const std::vector<Bar>& bars_data) const {
    MarketSnapshot market_snapshot;
    
    if (bars_data.empty()) {
        return market_snapshot;
    }

    // Use configurable ATR calculation bars
    const int atr_calculation_bars = config.strategy.atr_calculation_bars;
    if (static_cast<int>(bars_data.size()) < atr_calculation_bars + 2) {
        return market_snapshot;
    }

    // Extract inputs for technical indicators
    std::vector<double> highs, lows, closes, volumes;
    for (const auto& bar : bars_data) {
        highs.push_back(bar.h);
        lows.push_back(bar.l);
        closes.push_back(bar.c);
        volumes.push_back(bar.v);
    }

    // Compute technical indicators
    market_snapshot.atr = AlpacaTrader::Core::compute_atr(highs, lows, closes, atr_calculation_bars);
    market_snapshot.avg_atr = AlpacaTrader::Core::compute_atr(highs, lows, closes, atr_calculation_bars * config.strategy.average_atr_comparison_multiplier);
    market_snapshot.avg_vol = AlpacaTrader::Core::compute_average_volume(volumes, atr_calculation_bars, config.strategy.minimum_volume_threshold);

    // Set current and previous bars
    market_snapshot.curr = bars_data.back();
    if (bars_data.size() > 1) {
        market_snapshot.prev = bars_data[bars_data.size() - 2];
    }

    return market_snapshot;
}

AccountSnapshot MarketDataManager::create_account_snapshot() const {
    AccountSnapshot account_snapshot;
    
    account_snapshot.equity = account_manager.fetch_account_equity();
    
    SymbolRequest symbol_request{config.strategy.symbol};
    account_snapshot.pos_details = account_manager.fetch_position_details(symbol_request);
    account_snapshot.open_orders = account_manager.fetch_open_orders_count(symbol_request);
    
    // Calculate exposure percentage
    if (account_snapshot.equity > 0.0) {
        account_snapshot.exposure_pct = (std::abs(account_snapshot.pos_details.current_value) / account_snapshot.equity) * 100.0;
    } else {
        account_snapshot.exposure_pct = 0.0;
    }

    return account_snapshot;
}

QuoteData MarketDataManager::fetch_real_time_quote_data(const std::string& symbol) const {
    MarketDataLogs::log_market_data_fetch_table(symbol, config.logging.log_file);
    
    auto quote_data = api_manager.get_realtime_quotes(symbol);
    
    MarketDataLogs::log_market_data_result_table("Quote data fetched", true, quote_data.mid_price, config.logging.log_file);
    
    return quote_data;
}

} // namespace Core
} // namespace AlpacaTrader
