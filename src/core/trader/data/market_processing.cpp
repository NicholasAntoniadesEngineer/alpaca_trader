#include "market_processing.hpp"
#include "core/trader/analysis/indicators.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/logging/market_data_logs.hpp"
#include <cmath>

namespace AlpacaTrader {
namespace Core {

namespace MarketProcessing {

IndicatorInputs extract_inputs_from_bars(const std::vector<Bar>& bars) {
    IndicatorInputs inputs;
    inputs.highs.reserve(bars.size());
    inputs.lows.reserve(bars.size());
    inputs.closes.reserve(bars.size());
    inputs.volumes.reserve(bars.size());
    for (const Bar& b : bars) {
        inputs.highs.push_back(b.h);
        inputs.lows.push_back(b.l);
        inputs.closes.push_back(b.c);
        inputs.volumes.push_back(b.v);
    }
    return inputs;
}

ProcessedData compute_processed_data(const std::vector<Bar>& bars, const SystemConfig& cfg) {
    ProcessedData data;
    if (bars.empty()) return data;
    // Use configurable ATR calculation bars (new parameter) instead of deprecated atr_calculation_period
    const int atr_period = cfg.strategy.atr_calculation_bars;
    if (static_cast<int>(bars.size()) < atr_period + 2) return data;

    IndicatorInputs inputs = extract_inputs_from_bars(bars);
    data.atr = compute_atr(inputs.highs, inputs.lows, inputs.closes, atr_period);
    if (data.atr == 0.0) return data;
    data.avg_atr = compute_atr(inputs.highs, inputs.lows, inputs.closes, atr_period * cfg.strategy.average_atr_comparison_multiplier);
    data.avg_vol = compute_average_volume(inputs.volumes, atr_period, cfg.strategy.minimum_volume_threshold);
    
    
    data.curr = bars.back();
    data.prev = bars[bars.size() - 2];
    return data;
}

ProcessedData create_processed_data(const MarketSnapshot& market, const AccountSnapshot& account) {
    return ProcessedData(market, account);
}

void handle_market_close_positions(const ProcessedData& data, API::ApiManager& api_manager, const SystemConfig& config) {
    using AlpacaTrader::Logging::TradingLogs;
    
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

bool compute_technical_indicators(ProcessedData& data, const std::vector<Bar>& bars, const SystemConfig& config) {
    using AlpacaTrader::Logging::MarketDataLogs;
    
    MarketDataLogs::log_market_data_attempt_table("Computing indicators", config.logging.log_file);
    
    data = compute_processed_data(bars, config);
    
    if (data.atr == 0.0) {
        MarketDataLogs::log_market_data_result_table("Indicator computation failed", false, 0, config.logging.log_file);
        return false;
    }
    
    return true;
}

double calculate_exposure_percentage(double current_value, double equity) {
    if (equity <= 0.0) {
        return 0.0;
    }
    return (std::abs(current_value) / equity) * 100.0;
}

} // namespace MarketProcessing
} // namespace Core
} // namespace AlpacaTrader


