#ifndef DATA_STRUCTURES_HPP
#define DATA_STRUCTURES_HPP

#include <string>

// Trading signal constants
#define SIGNAL_BUY "buy"
#define SIGNAL_SELL "sell"

// Position side constants
#define POSITION_LONG "LONG"
#define POSITION_SHORT "SHORT"

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
namespace StrategyLogic {

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

} // namespace StrategyLogic

} // namespace Core
} // namespace AlpacaTrader

#endif // DATA_STRUCTURES_HPP
