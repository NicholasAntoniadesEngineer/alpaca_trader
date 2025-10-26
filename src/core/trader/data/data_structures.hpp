#ifndef DATA_STRUCTURES_HPP
#define DATA_STRUCTURES_HPP

#include <string>
#include "configs/system_config.hpp"

// Trading signal constants
#define SIGNAL_BUY "buy"
#define SIGNAL_SELL "sell"

// Position side constants
#define POSITION_LONG "LONG"
#define POSITION_SHORT "SHORT"

using AlpacaTrader::Config::TradingModeConfig;

namespace AlpacaTrader {
namespace Core {

struct Bar {
    double o, h, l, c;
    double v; /* volume */
    std::string t; /* timestamp */
};

struct QuoteData {
    double ask_price = 0.0;
    double bid_price = 0.0;
    double ask_size = 0.0;
    double bid_size = 0.0;
    std::string timestamp = "";
    double mid_price = 0.0; // Calculated as (ask + bid) / 2
};

struct PositionDetails {
    int qty = 0;
    double unrealized_pl = 0.0;
    double current_value = 0.0;
};

// Optional split views for multi-threading.
struct MarketSnapshot {
    double atr = 0.0;
    double avg_atr = 0.0;
    double avg_vol = 0.0;
    Bar curr{};
    Bar prev{};
};

struct AccountSnapshot {
    double equity = 0.0;
    PositionDetails pos_details{};
    int open_orders = 0;
    double exposure_pct = 0.0;
};

struct ProcessedData {
    double atr = 0.0;
    double avg_atr = 0.0;
    double avg_vol = 0.0;
    Bar curr;
    Bar prev;
    PositionDetails pos_details;
    int open_orders = 0;
    double exposure_pct = 0.0;
    bool is_doji = false;
    
    // Default constructor
    ProcessedData() = default;
    
    // Constructor from MarketSnapshot and AccountSnapshot
    ProcessedData(const MarketSnapshot& market, const AccountSnapshot& account)
        : atr(market.atr), avg_atr(market.avg_atr), avg_vol(market.avg_vol),
          curr(market.curr), prev(market.prev), pos_details(account.pos_details),
          open_orders(account.open_orders), exposure_pct(account.exposure_pct) {}
};

// Request objects (to avoid multi-parameter functions).
struct SymbolRequest {
    std::string symbol;
    explicit SymbolRequest(const std::string& s) : symbol(s) {}
};

struct BarRequest {
    std::string symbol;
    int limit;
    BarRequest(const std::string& s, int l) : symbol(s), limit(l) {}
};

struct OrderRequest {
    std::string side; // "buy" | "sell"
    int qty = 0;
    double tp = 0.0;
    double sl = 0.0;
    OrderRequest(const std::string& side_, int qty_, double tp_, double sl_)
        : side(side_), qty(qty_), tp(tp_), sl(sl_) {}
};

struct ClosePositionRequest {
    int current_qty = 0;
    explicit ClosePositionRequest(int q) : current_qty(q) {}
};

// Strategy logic data structures
struct SignalDecision {
    bool buy = false;
    bool sell = false;
    double signal_strength = 0.0;  // Signal strength (0.0 to 1.0)
    std::string signal_reason = ""; // Reason for signal/no signal
};

struct FilterResult {
    bool atr_pass = false;
    bool vol_pass = false;
    bool doji_pass = false;
    bool all_pass = false;
    double atr_ratio = 0.0;
    double vol_ratio = 0.0;
};

struct PositionSizing {
    int quantity = 0;
    double risk_amount = 0.0;
    double size_multiplier = 1.0;
    int risk_based_qty = 0;
    int exposure_based_qty = 0;
    int max_value_qty = 0;
    int buying_power_qty = 0;
};

struct ExitTargets {
    double stop_loss = 0.0;
    double take_profit = 0.0;
};

// Parameter structures for functions with many parameters
struct PositionSizingRequest {
    const ProcessedData& processed_data;
    double account_equity;
    int current_position_quantity;
    const StrategyConfig& strategy_configuration;
    double available_buying_power;
    
    PositionSizingRequest(const ProcessedData& data, double equity, int current_qty, const StrategyConfig& config, double buying_power)
        : processed_data(data), account_equity(equity), current_position_quantity(current_qty), 
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
    
    PositionSizingProcessRequest(const ProcessedData& data, double equity, int current_qty, double buying_power, const StrategyConfig& strategy_config, const TradingModeConfig& trading_mode_config)
        : processed_data(data), account_equity(equity), current_position_quantity(current_qty), 
          available_buying_power(buying_power), strategy_configuration(strategy_config), trading_mode_configuration(trading_mode_config) {}
};

} // namespace Core
} // namespace AlpacaTrader

#endif // DATA_STRUCTURES_HPP
