#include "market_bars_manager.hpp"
#include "trader/strategy_analysis/indicators.hpp"
#include <stdexcept>

namespace AlpacaTrader {
namespace Core {

MarketBarsManager::MarketBarsManager(const SystemConfig& cfg, API::ApiManager& api_mgr)
    : config(cfg), api_manager(api_mgr) {}

std::vector<Bar> MarketBarsManager::fetch_bars_data(const std::string& symbol) const {
    if (symbol.empty()) {
        throw std::runtime_error("Cannot fetch bars data: symbol is empty");
    }
    
    BarRequest bar_request(symbol, config.strategy.bars_to_fetch_for_calculations);
    return api_manager.get_recent_bars(bar_request);
}

bool MarketBarsManager::fetch_and_validate_bars(const std::string& symbol, std::vector<Bar>& bars_data) const {
    bars_data = fetch_bars_data(symbol);
    
    if (bars_data.empty()) {
        return false;
    }

    if (static_cast<int>(bars_data.size()) < config.strategy.bars_to_fetch_for_calculations) {
        return false;
    }

    // Validate individual bars
    for (const auto& bar : bars_data) {
        if (bar.close_price <= 0.0 || bar.high_price <= 0.0 || bar.low_price <= 0.0 || bar.open_price <= 0.0) {
            return false;
        }
        
        if (bar.high_price < bar.low_price || bar.high_price < bar.close_price || bar.low_price > bar.close_price) {
            return false;
        }
    }

    return true;
}

bool MarketBarsManager::compute_technical_indicators_from_bars(ProcessedData& processed_data, const std::vector<Bar>& bars_data) const {
    if (bars_data.empty()) {
        return false;
    }

    // Defensive: need at least 2 bars for prev/curr semantics downstream
    if (bars_data.size() < 2) {
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
    processed_data.atr = AlpacaTrader::Core::compute_atr(highs, lows, closes, config.strategy.atr_calculation_period, config.strategy.minimum_bars_for_atr_calculation);

    // Compute average volume
    processed_data.avg_vol = AlpacaTrader::Core::compute_average_volume(volumes, config.strategy.atr_calculation_period, config.strategy.minimum_volume_threshold);

    // Detect doji pattern
    processed_data.is_doji = AlpacaTrader::Core::detect_doji_pattern(current_bar.open_price, current_bar.high_price, current_bar.low_price, current_bar.close_price);

    if (processed_data.atr == 0.0) {
        return false;
    }

    return true;
}

MarketSnapshot MarketBarsManager::create_market_snapshot_from_bars(const std::vector<Bar>& bars_data) const {
    MarketSnapshot market_snapshot;
    
    if (bars_data.empty()) {
        return market_snapshot;
    }

    // Extract inputs for technical indicators
    std::vector<double> highs = extract_highs_from_bars(bars_data);
    std::vector<double> lows = extract_lows_from_bars(bars_data);
    std::vector<double> closes = extract_closes_from_bars(bars_data);
    std::vector<double> volumes = extract_volumes_from_bars(bars_data);

    // Use configurable ATR calculation bars, but ATR will adapt to available bars
    const int atr_calculation_bars = config.strategy.atr_calculation_bars;
    const int minimum_bars_for_atr_value = config.strategy.minimum_bars_for_atr_calculation;
    market_snapshot.atr = AlpacaTrader::Core::compute_atr(highs, lows, closes, atr_calculation_bars, minimum_bars_for_atr_value);
    
    int average_atr_period_value = atr_calculation_bars * config.strategy.average_atr_comparison_multiplier;
    market_snapshot.avg_atr = AlpacaTrader::Core::compute_atr(highs, lows, closes, average_atr_period_value, minimum_bars_for_atr_value);
    
    market_snapshot.avg_vol = AlpacaTrader::Core::compute_average_volume(volumes, atr_calculation_bars, config.strategy.minimum_volume_threshold);

    // Set current and previous bars
    market_snapshot.curr = bars_data.back();
    if (bars_data.size() > 1) {
        market_snapshot.prev = bars_data[bars_data.size() - 2];
    }
    
    // Store oldest bar timestamp for data accumulation time checking
    if (!bars_data.empty()) {
        market_snapshot.oldest_bar_timestamp = bars_data.front().timestamp;
    }

    return market_snapshot;
}

std::vector<Bar> MarketBarsManager::fetch_historical_market_data(const MarketDataFetchRequest& fetch_request) const {
    if (fetch_request.symbol.empty()) {
        throw std::runtime_error("Cannot fetch historical market data: symbol is empty");
    }
    
    if (fetch_request.bars_to_fetch <= 0) {
        throw std::runtime_error("Cannot fetch historical market data: bars_to_fetch must be greater than 0");
    }
    
    BarRequest bar_request{fetch_request.symbol, fetch_request.bars_to_fetch};
    return api_manager.get_recent_bars(bar_request);
}

bool MarketBarsManager::has_sufficient_bars_for_calculations(const std::vector<Bar>& historical_bars, int required_bars) const {
    if (required_bars <= 0) {
        return false;
    }
    
    int minimum_required_bars = required_bars + 2;
    
    if (static_cast<int>(historical_bars.size()) < minimum_required_bars) {
        return false;
    }
    
    return true;
}

ProcessedData MarketBarsManager::compute_processed_data_from_bars(const std::vector<Bar>& bars_data) const {
    ProcessedData processed_data_result;
    if (bars_data.empty()) {
        return processed_data_result;
    }
    
    // Use configurable ATR calculation bars for period calculations
    const int atr_calculation_period = config.strategy.atr_calculation_bars;
    if (static_cast<int>(bars_data.size()) < atr_calculation_period + 2) {
        return processed_data_result;
    }

    // Extract price data for calculations
    std::vector<double> highs = extract_highs_from_bars(bars_data);
    std::vector<double> lows = extract_lows_from_bars(bars_data);
    std::vector<double> closes = extract_closes_from_bars(bars_data);
    std::vector<double> volumes = extract_volumes_from_bars(bars_data);

    // Compute technical indicators
    int minimum_bars_for_atr_value = config.strategy.minimum_bars_for_atr_calculation;
    processed_data_result.atr = AlpacaTrader::Core::compute_atr(highs, lows, closes, atr_calculation_period, minimum_bars_for_atr_value);
    if (processed_data_result.atr == 0.0) {
        return processed_data_result;
    }
    
    processed_data_result.avg_atr = AlpacaTrader::Core::compute_atr(highs, lows, closes, atr_calculation_period * config.strategy.average_atr_comparison_multiplier, minimum_bars_for_atr_value);
    processed_data_result.avg_vol = AlpacaTrader::Core::compute_average_volume(volumes, atr_calculation_period, config.strategy.minimum_volume_threshold);
    
    // Defensive checks before accessing tail elements
    if (bars_data.size() < 2) {
        throw std::runtime_error("Insufficient bars for processed data tail access");
    }
    
    processed_data_result.curr = bars_data.back();
    processed_data_result.prev = bars_data[bars_data.size() - 2];
    
    return processed_data_result;
}

std::vector<double> MarketBarsManager::extract_highs_from_bars(const std::vector<Bar>& bars_data) const {
    std::vector<double> highs;
    highs.reserve(bars_data.size());
    for (const auto& bar : bars_data) {
        highs.push_back(bar.high_price);
    }
    return highs;
}

std::vector<double> MarketBarsManager::extract_lows_from_bars(const std::vector<Bar>& bars_data) const {
    std::vector<double> lows;
    lows.reserve(bars_data.size());
    for (const auto& bar : bars_data) {
        lows.push_back(bar.low_price);
    }
    return lows;
}

std::vector<double> MarketBarsManager::extract_closes_from_bars(const std::vector<Bar>& bars_data) const {
    std::vector<double> closes;
    closes.reserve(bars_data.size());
    for (const auto& bar : bars_data) {
        closes.push_back(bar.close_price);
    }
    return closes;
}

std::vector<double> MarketBarsManager::extract_volumes_from_bars(const std::vector<Bar>& bars_data) const {
    std::vector<double> volumes;
    volumes.reserve(bars_data.size());
    for (const auto& bar : bars_data) {
        volumes.push_back(bar.volume);
    }
    return volumes;
}

} // namespace Core
} // namespace AlpacaTrader
