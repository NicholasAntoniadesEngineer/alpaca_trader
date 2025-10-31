#include "bars_data_manager.hpp"
#include "core/logging/logs/market_data_logs.hpp"
#include "core/trader/analysis/indicators.hpp"
#include <stdexcept>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::MarketDataLogs;

BarsDataManager::BarsDataManager(const SystemConfig& cfg, API::ApiManager& api_mgr)
    : config(cfg), api_manager(api_mgr) {}

std::vector<Bar> BarsDataManager::fetch_bars_data(const std::string& symbol) const {
    try {
        BarRequest bar_request(symbol, config.strategy.bars_to_fetch_for_calculations);
        return api_manager.get_recent_bars(bar_request);
    } catch (const std::exception& bars_fetch_exception_error) {
        MarketDataLogs::log_market_data_failure_summary(
            symbol,
            "API Exception",
            bars_fetch_exception_error.what(),
            0,
            config.logging.log_file
        );
        return {};
    }
}

bool BarsDataManager::fetch_and_validate_bars(const std::string& symbol, std::vector<Bar>& bars_data) const {
    bars_data = fetch_bars_data(symbol);
    
    if (bars_data.empty()) {
        MarketDataLogs::log_market_data_failure_summary(
            symbol,
            "No Bars Received",
            "API returned empty bar data",
            0,
            config.logging.log_file
        );
        return false;
    }

    if (static_cast<int>(bars_data.size()) < config.strategy.bars_to_fetch_for_calculations) {
        MarketDataLogs::log_market_data_failure_summary(
            symbol,
            "Insufficient Bars",
            "Received " + std::to_string(bars_data.size()) + " bars, need " + std::to_string(config.strategy.bars_to_fetch_for_calculations),
            bars_data.size(),
            config.logging.log_file
        );
        return false;
    }

    // Validate individual bars
    for (const auto& bar : bars_data) {
        if (bar.close_price <= 0.0 || bar.high_price <= 0.0 || bar.low_price <= 0.0 || bar.open_price <= 0.0) {
            MarketDataLogs::log_market_data_failure_summary(
                symbol,
                "Invalid Bar Data",
                "Bar contains zero or negative prices",
                bars_data.size(),
                config.logging.log_file
            );
            return false;
        }
        
        if (bar.high_price < bar.low_price || bar.high_price < bar.close_price || bar.low_price > bar.close_price) {
            MarketDataLogs::log_market_data_failure_summary(
                symbol,
                "Invalid Bar Data",
                "Bar OHLC relationship violation",
                bars_data.size(),
                config.logging.log_file
            );
            return false;
        }
    }

    return true;
}

bool BarsDataManager::compute_technical_indicators_from_bars(ProcessedData& processed_data, const std::vector<Bar>& bars_data) const {
    MarketDataLogs::log_market_data_attempt_table("Computing indicators", config.logging.log_file);

    if (bars_data.empty()) {
        MarketDataLogs::log_market_data_result_table("Indicator computation failed - no bars", false, 0, config.logging.log_file);
        return false;
    }


    // Defensive: need at least 2 bars for prev/curr semantics downstream
    if (bars_data.size() < 2) {
        MarketDataLogs::log_market_data_result_table("Indicator computation failed - insufficient bars for tail access", false, bars_data.size(), config.logging.log_file);
        return false;
    }

    const Bar& current_bar = bars_data.back();
    processed_data.curr = current_bar;

    // Extract price data for calculations
    std::vector<double> highs = extract_highs_from_bars(bars_data);
    std::vector<double> lows = extract_lows_from_bars(bars_data);
    std::vector<double> closes = extract_closes_from_bars(bars_data);
    std::vector<double> volumes = extract_volumes_from_bars(bars_data);

    // Compute ATR
    processed_data.atr = AlpacaTrader::Core::compute_atr(highs, lows, closes, config.strategy.atr_calculation_period);

    // Compute average volume
    processed_data.avg_vol = AlpacaTrader::Core::compute_average_volume(volumes, config.strategy.atr_calculation_period, config.strategy.minimum_volume_threshold);

    // Detect doji pattern
    processed_data.is_doji = AlpacaTrader::Core::detect_doji_pattern(current_bar.open_price, current_bar.high_price, current_bar.low_price, current_bar.close_price);

    if (processed_data.atr == 0.0) {
        MarketDataLogs::log_market_data_result_table("Indicator computation failed - ATR is zero", false, 0, config.logging.log_file);
        return false;
    }

    MarketDataLogs::log_market_data_result_table("Indicator computation successful", true, processed_data.atr, config.logging.log_file);
    return true;
}

MarketSnapshot BarsDataManager::create_market_snapshot_from_bars(const std::vector<Bar>& bars_data) const {
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
    std::vector<double> highs = extract_highs_from_bars(bars_data);
    std::vector<double> lows = extract_lows_from_bars(bars_data);
    std::vector<double> closes = extract_closes_from_bars(bars_data);
    std::vector<double> volumes = extract_volumes_from_bars(bars_data);

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

std::vector<Bar> BarsDataManager::fetch_historical_market_data(const MarketDataFetchRequest& fetch_request) const {
    MarketDataLogs::log_market_data_fetch_table(fetch_request.symbol, config.logging.log_file);
    
    try {
        BarRequest bar_request{fetch_request.symbol, fetch_request.bars_to_fetch};
        auto historical_bars = api_manager.get_recent_bars(bar_request);
        MarketDataLogs::log_market_data_result_table("Bars fetched", true, historical_bars.size(), config.logging.log_file);
        return historical_bars;
    } catch (const std::exception& historical_bars_exception_error) {
        MarketDataLogs::log_market_data_failure_summary(fetch_request.symbol, "API Exception", historical_bars_exception_error.what(), 0, config.logging.log_file);
        return {};
    }
}

bool BarsDataManager::has_sufficient_bars_for_calculations(const std::vector<Bar>& historical_bars, int required_bars) const {
    int minimum_required_bars = required_bars + 2;
    
    if (static_cast<int>(historical_bars.size()) < minimum_required_bars) {
        MarketDataLogs::log_market_data_result_table("Insufficient bars for calculations", false, historical_bars.size(), config.logging.log_file);
        return false;
    }
    
    MarketDataLogs::log_market_data_result_table("Sufficient bars for calculations", true, historical_bars.size(), config.logging.log_file);
    return true;
}

std::vector<double> BarsDataManager::extract_highs_from_bars(const std::vector<Bar>& bars_data) const {
    std::vector<double> highs;
    highs.reserve(bars_data.size());
    for (const auto& bar : bars_data) {
        highs.push_back(bar.high_price);
    }
    return highs;
}

std::vector<double> BarsDataManager::extract_lows_from_bars(const std::vector<Bar>& bars_data) const {
    std::vector<double> lows;
    lows.reserve(bars_data.size());
    for (const auto& bar : bars_data) {
        lows.push_back(bar.low_price);
    }
    return lows;
}

std::vector<double> BarsDataManager::extract_closes_from_bars(const std::vector<Bar>& bars_data) const {
    std::vector<double> closes;
    closes.reserve(bars_data.size());
    for (const auto& bar : bars_data) {
        closes.push_back(bar.close_price);
    }
    return closes;
}

std::vector<double> BarsDataManager::extract_volumes_from_bars(const std::vector<Bar>& bars_data) const {
    std::vector<double> volumes;
    volumes.reserve(bars_data.size());
    for (const auto& bar : bars_data) {
        volumes.push_back(bar.volume);
    }
    return volumes;
}

} // namespace Core
} // namespace AlpacaTrader
