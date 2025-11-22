#include "market_bars_manager.hpp"
#include "trader/strategy_analysis/indicators.hpp"
#include "logging/logger/logging_macros.hpp"
#include "logging/logs/websocket_logs.hpp"
#include <stdexcept>
#include <sstream>

namespace AlpacaTrader {
namespace Core {

MarketBarsManager::MarketBarsManager(const SystemConfig& cfg, API::ApiManager& api_mgr)
    : config(cfg), api_manager(api_mgr) {}

std::vector<Bar> MarketBarsManager::fetch_bars_data(const std::string& symbol) const {
    if (symbol.empty()) {
        throw std::runtime_error("Cannot fetch bars data: symbol is empty");
    }
    
    BarRequest bar_request(symbol, config.strategy.bars_to_fetch_for_calculations, config.strategy.minimum_bars_for_atr_calculation);
    std::vector<Bar> fetched_bars_result = api_manager.get_recent_bars(bar_request);
    
    
    return fetched_bars_result;
}

bool MarketBarsManager::fetch_and_validate_bars(const std::string& symbol, std::vector<Bar>& bars_data) const {
    bars_data = fetch_bars_data(symbol);
    
    if (bars_data.empty()) {
        return false;
    }

    // CRITICAL: Only require minimum bars (3) instead of full bars_to_fetch_for_calculations (25)
    // This allows processing to start with fewer bars while still fetching more when available
    if (static_cast<int>(bars_data.size()) < config.strategy.minimum_bars_for_atr_calculation) {
        return false;
    }

    // Validate individual bars - ensure all bars have valid positive prices
    for (const auto& bar : bars_data) {
        if (bar.close_price <= 0.0 || bar.high_price <= 0.0 || bar.low_price <= 0.0 || bar.open_price <= 0.0) {
            return false;
        }
        // Ensure logical relationships between prices
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

    // CRITICAL: Validate before accessing back()
    if (bars_data.empty()) {
        return false;
    }

    const Bar& current_bar = bars_data.back();
    processed_data.curr = current_bar;

    // Calculate maximum bars needed for all calculations
    // avg_atr uses: atr_calculation_bars * average_atr_comparison_multiplier + 1 (for true range calculation)
    const int atr_calculation_bars_value = config.strategy.atr_calculation_bars;
    const int average_atr_period_bars_value = atr_calculation_bars_value * config.strategy.average_atr_comparison_multiplier;
    const int max_bars_needed_for_calculations_value = average_atr_period_bars_value + 1;
    
    // Extract only the last N bars needed for calculations (optimize buffer usage)
    // This ensures we use only recent bars and avoid unnecessary processing of old data
    std::vector<Bar> bars_for_calculation_data;
    if (static_cast<int>(bars_data.size()) > max_bars_needed_for_calculations_value) {
        size_t start_index_value = bars_data.size() - static_cast<size_t>(max_bars_needed_for_calculations_value);
        bars_for_calculation_data = std::vector<Bar>(bars_data.begin() + start_index_value, bars_data.end());
    } else {
        bars_for_calculation_data = bars_data;
    }
    
    // Extract price data for calculations (using only bars needed)
    std::vector<double> highs = extract_highs_from_bars(bars_for_calculation_data);
    std::vector<double> lows = extract_lows_from_bars(bars_for_calculation_data);
    std::vector<double> closes = extract_closes_from_bars(bars_for_calculation_data);
    std::vector<double> volumes = extract_volumes_from_bars(bars_for_calculation_data);

    // Compute ATR - calculates continuously, can be 0.0 during initial accumulation
    processed_data.atr = AlpacaTrader::Core::compute_atr(highs, lows, closes, atr_calculation_bars_value, config.strategy.minimum_bars_for_atr_calculation);

    // Compute average volume
    processed_data.avg_vol = AlpacaTrader::Core::compute_average_volume(volumes, atr_calculation_bars_value, config.strategy.minimum_volume_threshold);

    // Detect doji pattern
    processed_data.is_doji = AlpacaTrader::Core::detect_doji_pattern(current_bar.open_price, current_bar.high_price, current_bar.low_price, current_bar.close_price);

    // ATR can be 0.0 during initial accumulation - allow continuous calculation
    // Trading will be blocked by data accumulation time check until sufficient data
    return true;
}

MarketSnapshot MarketBarsManager::create_market_snapshot_from_bars(const std::vector<Bar>& bars_data) const {
    // Top-level try-catch to prevent segfault
    MarketSnapshot market_snapshot;
    
    try {
    if (bars_data.empty()) {
        return market_snapshot;
    }

    // Calculate maximum bars needed for all calculations
    // avg_atr uses: atr_calculation_bars * average_atr_comparison_multiplier + 1 (for true range calculation)
    const int atr_calculation_bars_value = config.strategy.atr_calculation_bars;
    const int minimum_bars_for_atr_value = config.strategy.minimum_bars_for_atr_calculation;
    const int average_atr_period_bars_value = atr_calculation_bars_value * config.strategy.average_atr_comparison_multiplier;
    const int max_bars_needed_for_calculations_value = average_atr_period_bars_value + 1;
    
    // Extract only the last N bars needed for calculations (optimize buffer usage)
    // This ensures we use only recent bars and avoid unnecessary processing of old data
    std::vector<Bar> bars_for_calculation_data;
    if (static_cast<int>(bars_data.size()) > max_bars_needed_for_calculations_value) {
        size_t start_index_value = bars_data.size() - static_cast<size_t>(max_bars_needed_for_calculations_value);
        bars_for_calculation_data = std::vector<Bar>(bars_data.begin() + start_index_value, bars_data.end());
    } else {
        bars_for_calculation_data = bars_data;
    }
    
    // Extract inputs for technical indicators (using only bars needed)
    std::vector<double> highs = extract_highs_from_bars(bars_for_calculation_data);
    std::vector<double> lows = extract_lows_from_bars(bars_for_calculation_data);
    std::vector<double> closes = extract_closes_from_bars(bars_for_calculation_data);
    std::vector<double> volumes = extract_volumes_from_bars(bars_for_calculation_data);

    market_snapshot.atr = AlpacaTrader::Core::compute_atr(highs, lows, closes, atr_calculation_bars_value, minimum_bars_for_atr_value);
    market_snapshot.avg_atr = AlpacaTrader::Core::compute_atr(highs, lows, closes, average_atr_period_bars_value, minimum_bars_for_atr_value);
    market_snapshot.avg_vol = AlpacaTrader::Core::compute_average_volume(volumes, atr_calculation_bars_value, config.strategy.minimum_volume_threshold);

    if (market_snapshot.atr == 0.0) {
        AlpacaTrader::Logging::log_message("ATR calculation returned zero - insufficient bars (" + std::to_string(bars_for_calculation_data.size()) + " available, " + std::to_string(minimum_bars_for_atr_value) + " required)", "");
    }

    // Set current and previous bars - CRITICAL: Add try-catch and validate bounds
    try {
        if (bars_data.empty()) {
        return market_snapshot;
    }

        const Bar& latest_bar = bars_data.back();
        
        // CRITICAL: Validate bar data before assigning
        if (latest_bar.open_price > 0.0 && (latest_bar.high_price == 0.0 || latest_bar.low_price == 0.0 || latest_bar.close_price == 0.0)) {
            // Bar data is incomplete - this is a critical error that should not be silently ignored
            std::string error_msg = "CRITICAL: create_market_snapshot_from_bars - Bar data incomplete - O:" + 
                std::to_string(latest_bar.open_price) + " H:" + std::to_string(latest_bar.high_price) + 
                " L:" + std::to_string(latest_bar.low_price) + " C:" + std::to_string(latest_bar.close_price);
            // Re-throw to be caught by coordinator exception handler
            throw std::runtime_error(error_msg);
        }
        
        // Assign bar data - this should copy all fields correctly
        market_snapshot.curr.open_price = latest_bar.open_price;
        market_snapshot.curr.high_price = latest_bar.high_price;
        market_snapshot.curr.low_price = latest_bar.low_price;
        market_snapshot.curr.close_price = latest_bar.close_price;
        market_snapshot.curr.volume = latest_bar.volume;
        market_snapshot.curr.timestamp = latest_bar.timestamp;
        
    if (bars_data.size() > 1) {
            size_t prev_index = bars_data.size() - 2;
            if (prev_index < bars_data.size()) {
                const Bar& prev_bar = bars_data[prev_index];
                market_snapshot.prev.open_price = prev_bar.open_price;
                market_snapshot.prev.high_price = prev_bar.high_price;
                market_snapshot.prev.low_price = prev_bar.low_price;
                market_snapshot.prev.close_price = prev_bar.close_price;
                market_snapshot.prev.volume = prev_bar.volume;
                market_snapshot.prev.timestamp = prev_bar.timestamp;
            }
        }
        
        // Store oldest bar timestamp for data accumulation time checking
        if (!bars_data.empty()) {
            market_snapshot.oldest_bar_timestamp = bars_data.front().timestamp;
        }
    } catch (const std::exception& bar_access_exception_error) {
        // Re-throw to be caught by coordinator - don't silently return partial data
        throw;
    } catch (...) {
        // Re-throw to be caught by coordinator - don't silently return partial data
        throw;
    }
    } catch (const std::exception& top_level_exception_error) {
        // Return empty snapshot on top-level exception
        return MarketSnapshot();
    } catch (...) {
        // Return empty snapshot on unknown exception
        return MarketSnapshot();
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
    
    BarRequest bar_request{fetch_request.symbol, fetch_request.bars_to_fetch, config.strategy.minimum_bars_for_atr_calculation};
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
    // Top-level try-catch to prevent segfault
    ProcessedData processed_data_result;
    
    try {
    if (bars_data.empty()) {
        return processed_data_result;
    }
    
    // Calculate maximum bars needed for all calculations
    // avg_atr uses: atr_calculation_bars * average_atr_comparison_multiplier + 1 (for true range calculation)
    const int atr_calculation_bars_value = config.strategy.atr_calculation_bars;
    const int minimum_bars_for_atr_value = config.strategy.minimum_bars_for_atr_calculation;
    const int average_atr_period_bars_value = atr_calculation_bars_value * config.strategy.average_atr_comparison_multiplier;
    const int max_bars_needed_for_calculations_value = average_atr_period_bars_value + 1;
    
    // Extract only the last N bars needed for calculations (optimize buffer usage)
    // This ensures we use only recent bars and avoid unnecessary processing of old data
    std::vector<Bar> bars_for_calculation_data;
    if (static_cast<int>(bars_data.size()) > max_bars_needed_for_calculations_value) {
        size_t start_index_value = bars_data.size() - static_cast<size_t>(max_bars_needed_for_calculations_value);
        bars_for_calculation_data = std::vector<Bar>(bars_data.begin() + start_index_value, bars_data.end());
    } else {
        bars_for_calculation_data = bars_data;
    }

    // Extract price data for calculations (using only bars needed)
    std::vector<double> highs = extract_highs_from_bars(bars_for_calculation_data);
    std::vector<double> lows = extract_lows_from_bars(bars_for_calculation_data);
    std::vector<double> closes = extract_closes_from_bars(bars_for_calculation_data);
    std::vector<double> volumes = extract_volumes_from_bars(bars_for_calculation_data);

    // Compute technical indicators - ATR calculates continuously, can be 0.0 during initial accumulation
    processed_data_result.atr = AlpacaTrader::Core::compute_atr(highs, lows, closes, atr_calculation_bars_value, minimum_bars_for_atr_value);
    
    processed_data_result.avg_atr = AlpacaTrader::Core::compute_atr(highs, lows, closes, average_atr_period_bars_value, minimum_bars_for_atr_value);
    processed_data_result.avg_vol = AlpacaTrader::Core::compute_average_volume(volumes, atr_calculation_bars_value, config.strategy.minimum_volume_threshold);
    
    // Defensive checks before accessing tail elements - CRITICAL: Add comprehensive bounds checking
    try {
    if (bars_data.size() < 2) {
        throw std::runtime_error("Insufficient bars for processed data tail access");
    }
    
        // Validate indices before accessing
        size_t bars_size_value = bars_data.size();
        if (bars_size_value == 0) {
            throw std::runtime_error("Empty bars_data vector");
        }
        
        // Access current bar (last element)
    processed_data_result.curr = bars_data.back();
        
        // Access previous bar (second to last) - CRITICAL: Validate index
        size_t prev_index_value = bars_size_value - 2;
        if (prev_index_value >= bars_size_value) {
            throw std::runtime_error("Invalid previous bar index: " + std::to_string(prev_index_value) + " >= " + std::to_string(bars_size_value));
        }
        processed_data_result.prev = bars_data[prev_index_value];
        
        // Store oldest bar timestamp for data accumulation time checking
        if (!bars_data.empty()) {
            processed_data_result.oldest_bar_timestamp = bars_data.front().timestamp;
        }
    } catch (const std::exception& bar_access_exception_error) {
        // Log error and re-throw to ensure system fails hard on invalid data
        throw std::runtime_error("Exception accessing bars_data elements: " + std::string(bar_access_exception_error.what()));
    } catch (...) {
        throw std::runtime_error("Unknown exception accessing bars_data elements");
    }
    } catch (const std::exception& top_level_exception_error) {
        throw std::runtime_error("CRITICAL: Top-level exception in compute_processed_data_from_bars: " + std::string(top_level_exception_error.what()));
    } catch (...) {
        throw std::runtime_error("CRITICAL: Unknown top-level exception in compute_processed_data_from_bars - segfault prevented");
    }
    
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
