#ifndef DATA_STRUCTURES_HPP
#define DATA_STRUCTURES_HPP

#include <string>
#include <deque>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
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

    Bar() : open_price(0.0), high_price(0.0), low_price(0.0), close_price(0.0), volume(0.0), timestamp("") {}
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

    PositionDetails() : position_quantity(0), unrealized_pl(0.0), current_value(0.0) {}
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

    AccountSnapshot() : equity(0.0), pos_details(), open_orders(0), exposure_pct(0.0) {}
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

    SignalDecision() : buy(false), sell(false), signal_strength(0.0), signal_reason("") {}
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
    double quantity;
    double risk_amount;
    double size_multiplier;
    double risk_based_qty;
    double exposure_based_qty;
    double max_value_qty;
    double buying_power_qty;
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
    const TradingModeConfig& trading_mode_configuration;
    
    PositionSizingRequest(const ProcessedData& data, double equity, int current_position_qty, const StrategyConfig& config, double buying_power, const TradingModeConfig& trading_mode_config)
        : processed_data(data), account_equity(equity), current_position_quantity(current_position_qty), 
          strategy_configuration(config), available_buying_power(buying_power), trading_mode_configuration(trading_mode_config) {}
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

// ========================================================================
// ORDER EXECUTION RESULT STRUCTURES
// ========================================================================

struct OrderExecutionResult {
    bool order_successful;
    std::string order_id;
    std::string execution_status;
    double executed_quantity;
    double executed_price;
    std::string error_message;

    OrderExecutionResult()
        : order_successful(false), order_id(""), execution_status(""),
          executed_quantity(0.0), executed_price(0.0), error_message("") {}
};

struct PositionClosureResult {
    bool closure_successful;
    std::string closure_reason;
    double closed_quantity;
    double closure_price;
    double profit_loss_amount;
    std::string error_message;

    PositionClosureResult()
        : closure_successful(false), closure_reason(""), closed_quantity(0.0),
          closure_price(0.0), profit_loss_amount(0.0), error_message("") {}
};

struct PositionSizingProcessRequest {
    const ProcessedData& processed_data;
    double account_equity;
    int current_position_quantity;
    double available_buying_power;
    double signal_strength;  // Signal strength (0.0 to 1.0) for hybrid evaluation
    const StrategyConfig& strategy_configuration;
    const TradingModeConfig& trading_mode_configuration;

    PositionSizingProcessRequest(const ProcessedData& data, double equity, int current_position_qty, double buying_power, double signal_strength_val, const StrategyConfig& strategy_config, const TradingModeConfig& trading_mode_config)
        : processed_data(data), account_equity(equity), current_position_quantity(current_position_qty),
          available_buying_power(buying_power), signal_strength(signal_strength_val),
          strategy_configuration(strategy_config), trading_mode_configuration(trading_mode_config) {}
};

// ========================================================================
// MTH-TS MULTI-TIMEFRAME DATA STRUCTURES
// ========================================================================

// Multi-timeframe bar data structure
struct MultiTimeframeBar {
    double open_price;
    double high_price;
    double low_price;
    double close_price;
    double volume;
    double spread;  // bid-ask spread percentage
    std::string timestamp;
    std::chrono::system_clock::time_point timestamp_tp;

    MultiTimeframeBar() : open_price(0.0), high_price(0.0), low_price(0.0), close_price(0.0),
                         volume(0.0), spread(0.0), timestamp(), timestamp_tp() {}

    MultiTimeframeBar(double o, double h, double l, double c, double v, double s, const std::string& ts)
        : open_price(o), high_price(h), low_price(l), close_price(c), volume(v), spread(s), timestamp(ts) {
        // Parse timestamp to time_point for easier time-based operations
        // Handle both Unix millisecond timestamps (digits only) and ISO strings
        try {
            bool is_unix_ms = !ts.empty() && std::all_of(ts.begin(), ts.end(), ::isdigit);
            if (is_unix_ms) {
                try {
                    long long unix_milliseconds = std::stoll(ts);
                    // Validate timestamp is reasonable (not negative, not too far in future)
                    if (unix_milliseconds > 0 && unix_milliseconds < 9999999999999LL) { // reasonable range
                        // Polygon sends timestamps in milliseconds, convert to system_clock time_point
                        auto duration_ms = std::chrono::milliseconds(unix_milliseconds);
                        timestamp_tp = std::chrono::time_point<std::chrono::system_clock>(duration_ms);
                    } else {
                        // Invalid timestamp - leave uninitialized
                        timestamp_tp = std::chrono::system_clock::time_point{};
                    }
                } catch (const std::invalid_argument&) {
                    // Not a valid number - leave uninitialized
                    timestamp_tp = std::chrono::system_clock::time_point{};
                } catch (const std::out_of_range&) {
                    // Number too large - leave uninitialized
                    timestamp_tp = std::chrono::system_clock::time_point{};
                } catch (const std::exception&) {
                    // Any other error - leave uninitialized
                    timestamp_tp = std::chrono::system_clock::time_point{};
                }
            } else {
                // Try parsing as ISO string format
                try {
                    std::tm tm = {};
                    std::istringstream ss(ts);
                    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
                    if (!ss.fail()) {
                        timestamp_tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                    } else {
                        // Parsing failed - leave uninitialized
                        timestamp_tp = std::chrono::system_clock::time_point{};
                    }
                } catch (const std::exception&) {
                    // Parsing failed - leave uninitialized
                    timestamp_tp = std::chrono::system_clock::time_point{};
                }
            }
        } catch (const std::exception&) {
            // Any error in timestamp parsing - ensure timestamp_tp is in a safe state
            timestamp_tp = std::chrono::system_clock::time_point{};
        }
    }
};

// Timeframe enumeration for MTH-TS strategy
enum class MthTsTimeframe {
    SECOND_1,
    MINUTE_1,
    MINUTE_30,
    DAILY
};

// MTH-TS Timeframe Analysis Structure
struct MthTsTimeframeAnalysis {
    bool daily_bias;
    bool thirty_min_confirmation;
    bool one_min_trigger;
    bool one_sec_execution;

    MthTsTimeframeAnalysis() : daily_bias(false), thirty_min_confirmation(false),
                              one_min_trigger(false), one_sec_execution(false) {}
};

// Multi-timeframe data container
struct MultiTimeframeData {
    std::deque<MultiTimeframeBar> second_bars;    // 1-second bars (last 300 for 5-min history)
    std::deque<MultiTimeframeBar> minute_bars;    // 1-minute bars (last 1800 for 30-hour history)
    std::deque<MultiTimeframeBar> thirty_min_bars; // 30-minute bars (last 336 for 7-day history)
    std::deque<MultiTimeframeBar> daily_bars;     // Daily bars (last 30 for 30-day history)

    // Technical indicators for each timeframe
    struct TechnicalIndicators {
        double ema;           // EMA value
        double adx;           // ADX value
        double rsi;           // RSI value
        double atr;           // ATR value
        double volume_ma;     // Volume moving average
        double spread_avg;    // Average spread

        TechnicalIndicators() : ema(0.0), adx(0.0), rsi(0.0), atr(0.0), volume_ma(0.0), spread_avg(0.0) {}
    };

    TechnicalIndicators daily_indicators;
    TechnicalIndicators thirty_min_indicators;
    TechnicalIndicators minute_indicators;
    TechnicalIndicators second_indicators;

    // Bias flags for hierarchical analysis
    bool daily_bullish_bias;
    bool thirty_min_confirmation;
    bool minute_trigger_signal;
    bool second_execution_ready;

    // Propagation scores for hybrid evaluation (0.0 to 1.0)
    double minute_to_thirty_min_propagation_score;
    double second_to_minute_propagation_score;

    // Consecutive bar tracking to prevent bottom-up whipsaws
    int consecutive_minute_bars_aligned;
    int consecutive_second_bars_aligned;

    MultiTimeframeData()
        : daily_bullish_bias(false), thirty_min_confirmation(false),
          minute_trigger_signal(false), second_execution_ready(false),
          minute_to_thirty_min_propagation_score(0.0), second_to_minute_propagation_score(0.0),
          consecutive_minute_bars_aligned(0), consecutive_second_bars_aligned(0) {}
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
