#ifndef DATA_STRUCTURES_HPP
#define DATA_STRUCTURES_HPP

#include <string>
#include "configs/system_config.hpp"

using AlpacaTrader::Config::TradingModeConfig;

namespace AlpacaTrader {
namespace Core {

struct Bar {
    double open_price;
    double high_price;
    double low_price;
    double close_price;
    double volume;
    std::string timestamp;
};

struct QuoteData {
    double ask_price;
    double bid_price;
    double ask_size;
    double bid_size;
    std::string timestamp;
    double mid_price;
};

struct PositionDetails {
    int position_quantity;
    double unrealized_pl;
    double current_value;
};

// Optional split views for multi-threading.
struct MarketSnapshot {
    double atr;
    double avg_atr;
    double avg_vol;
    Bar curr;
    Bar prev;
    std::string oldest_bar_timestamp;
};

struct AccountSnapshot {
    double equity;
    PositionDetails pos_details;
    int open_orders;
    double exposure_pct;
};

struct ProcessedData {
    double atr;
    double avg_atr;
    double avg_vol;
    Bar curr;
    Bar prev;
    PositionDetails pos_details;
    int open_orders;
    double exposure_pct;
    bool is_doji;
    std::string oldest_bar_timestamp;
    
    ProcessedData()
        : atr(0.0), avg_atr(0.0), avg_vol(0.0), curr(), prev(), pos_details(), open_orders(0), exposure_pct(0.0), is_doji(false), oldest_bar_timestamp() {}
    
    ProcessedData(const MarketSnapshot& market, const AccountSnapshot& account)
        : atr(market.atr), avg_atr(market.avg_atr), avg_vol(market.avg_vol),
          pos_details(account.pos_details),
          open_orders(account.open_orders), exposure_pct(account.exposure_pct), is_doji(false),
          oldest_bar_timestamp(market.oldest_bar_timestamp) {
        // CRITICAL: Explicitly copy all Bar fields to avoid struct copy issues
        curr.open_price = market.curr.open_price;
        curr.high_price = market.curr.high_price;
        curr.low_price = market.curr.low_price;
        curr.close_price = market.curr.close_price;
        curr.volume = market.curr.volume;
        curr.timestamp = market.curr.timestamp;
        
        prev.open_price = market.prev.open_price;
        prev.high_price = market.prev.high_price;
        prev.low_price = market.prev.low_price;
        prev.close_price = market.prev.close_price;
        prev.volume = market.prev.volume;
        prev.timestamp = market.prev.timestamp;
    }
};

// Request objects (to avoid multi-parameter functions).
struct SymbolRequest {
    std::string symbol;
    explicit SymbolRequest(const std::string& input_symbol) : symbol(input_symbol) {}
};

struct BarRequest {
    std::string symbol;
    int limit;
    int minimum_bars_required;
    BarRequest(const std::string& input_symbol, int bar_limit, int minimum_bars_required_param) 
        : symbol(input_symbol), limit(bar_limit), minimum_bars_required(minimum_bars_required_param) {}
};

struct OrderRequest {
    std::string side;
    int position_quantity;
    double take_profit_price;
    double stop_loss_price;
    OrderRequest(const std::string& side_param, int position_qty_param, double take_profit_param, double stop_loss_param)
        : side(side_param), position_quantity(position_qty_param), take_profit_price(take_profit_param), stop_loss_price(stop_loss_param) {}
};

struct ClosePositionRequest {
    int current_position_quantity;
    explicit ClosePositionRequest(int position_qty_param) : current_position_quantity(position_qty_param) {}
};

// Strategy logic data structures
struct SignalDecision {
    bool buy;
    bool sell;
    double signal_strength;
    std::string signal_reason;
};

struct FilterResult {
    bool atr_pass;
    bool vol_pass;
    bool doji_pass;
    bool all_pass;
    double atr_ratio;
    double vol_ratio;
};

struct PositionSizing {
    int quantity;
    double risk_amount;
    double size_multiplier;
    int risk_based_qty;
    int exposure_based_qty;
    int max_value_qty;
    int buying_power_qty;
};

struct ExitTargets {
    double stop_loss;
    double take_profit;
};

// Parameter structures for functions with many parameters
struct PositionSizingRequest {
    const ProcessedData& processed_data;
    double account_equity;
    int current_position_quantity;
    const StrategyConfig& strategy_configuration;
    double available_buying_power;
    
    PositionSizingRequest(const ProcessedData& data, double equity, int current_position_qty, const StrategyConfig& config, double buying_power)
        : processed_data(data), account_equity(equity), current_position_quantity(current_position_qty), 
          strategy_configuration(config), available_buying_power(buying_power) {}
};

struct ExitTargetsRequest {
    const std::string& position_side;
    double entry_price;
    double risk_amount;
    const StrategyConfig& strategy_configuration;
    
    ExitTargetsRequest(const std::string& side, double entry_price, double risk_amount, const StrategyConfig& config)
        : position_side(side), entry_price(entry_price), risk_amount(risk_amount), 
          strategy_configuration(config) {}
};

struct PositionSizingProcessRequest {
    const ProcessedData& processed_data;
    double account_equity;
    int current_position_quantity;
    double available_buying_power;
    const StrategyConfig& strategy_configuration;
    const TradingModeConfig& trading_mode_configuration;
    
    PositionSizingProcessRequest(const ProcessedData& data, double equity, int current_position_qty, double buying_power, const StrategyConfig& strategy_config, const TradingModeConfig& trading_mode_config)
        : processed_data(data), account_equity(equity), current_position_quantity(current_position_qty), 
          available_buying_power(buying_power), strategy_configuration(strategy_config), trading_mode_configuration(trading_mode_config) {}
};

// Market data thread parameter structures
struct MarketDataFetchRequest {
    std::string symbol;
    int bars_to_fetch;
    int atr_calculation_bars;
    
    MarketDataFetchRequest(const std::string& input_symbol, int bars_to_fetch_param, int atr_calculation_bars_param)
        : symbol(input_symbol), bars_to_fetch(bars_to_fetch_param), atr_calculation_bars(atr_calculation_bars_param) {}
};

struct QuoteDataProcessingRequest {
    const std::string& symbol;
    const std::string& timestamp;
    double mid_price;
    double bid_price;
    double ask_price;
    int bid_size;
    int ask_size;
    int freshness_threshold_seconds;
    
    QuoteDataProcessingRequest(const std::string& symbol, const std::string& timestamp, double mid_price, 
                              double bid_price, double ask_price, int bid_size, int ask_size, int freshness_threshold_seconds)
        : symbol(symbol), timestamp(timestamp), mid_price(mid_price), bid_price(bid_price), 
          ask_price(ask_price), bid_size(bid_size), ask_size(ask_size), freshness_threshold_seconds(freshness_threshold_seconds) {}
};

struct CsvLoggingRequest {
    const std::string& symbol;
    const std::string& timestamp;
    const ProcessedData& processed_data;
    int logging_interval_seconds;
    std::chrono::steady_clock::time_point last_log_time;
    
    CsvLoggingRequest(const std::string& symbol, const std::string& timestamp, const ProcessedData& processed_data, 
                     int logging_interval_seconds, std::chrono::steady_clock::time_point last_log_time)
        : symbol(symbol), timestamp(timestamp), processed_data(processed_data), 
          logging_interval_seconds(logging_interval_seconds), last_log_time(last_log_time) {}
};

} // namespace Core
} // namespace AlpacaTrader

#endif // DATA_STRUCTURES_HPP
