#ifndef DATA_STRUCTURES_HPP
#define DATA_STRUCTURES_HPP

#include <string>

namespace AlpacaTrader {
namespace Core {

struct Bar {
    double o, h, l, c;
    long long v; /* volume */
};

struct PositionDetails {
    int qty = 0;
    double unrealized_pl = 0.0;
    double current_value = 0.0;
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

} // namespace Core
} // namespace AlpacaTrader

#endif // DATA_STRUCTURES_HPP
