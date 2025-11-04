#include "market_data_validator.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace AlpacaTrader {
namespace Core {

MarketDataValidator::MarketDataValidator(const SystemConfig& cfg) : config(cfg) {}

bool MarketDataValidator::validate_market_snapshot(const MarketSnapshot& market_snapshot) const {
    // Check if this is a default/empty MarketSnapshot (no data available)
    // ATR can be 0.0 during initial accumulation - only reject if ALL data is zero
    if (market_snapshot.curr.open_price == 0.0 && market_snapshot.curr.high_price == 0.0 && 
        market_snapshot.curr.low_price == 0.0 && market_snapshot.curr.close_price == 0.0) {
        return false;
    }

    // Validate price data - this is critical
    if (!validate_price_data(market_snapshot.curr)) {
        return false;
    }

    // Validate technical indicators - ATR can be 0.0, only reject NaN/infinite
    if (!validate_technical_indicators(market_snapshot)) {
        return false;
    }

    return true;
}

bool MarketDataValidator::validate_account_snapshot(const AccountSnapshot& account_snapshot) const {
    // Validate equity
    if (account_snapshot.equity <= 0.0) {
        return false;
    }

    // Validate position data
    if (!validate_position_data(account_snapshot.pos_details)) {
        return false;
    }

    return true;
}

bool MarketDataValidator::validate_processed_data(const ProcessedData& processed_data) const {
    // Validate market data components
    MarketSnapshot market_snapshot;
    market_snapshot.atr = processed_data.atr;
    market_snapshot.avg_atr = processed_data.avg_atr;
    market_snapshot.avg_vol = processed_data.avg_vol;
    market_snapshot.curr = processed_data.curr;
    market_snapshot.prev = processed_data.prev;

    if (!validate_market_snapshot(market_snapshot)) {
        return false;
    }

    // Validate account data components
    AccountSnapshot account_snapshot;
    account_snapshot.equity = 0.0; // Will be validated separately
    account_snapshot.pos_details = processed_data.pos_details;
    account_snapshot.open_orders = processed_data.open_orders;
    account_snapshot.exposure_pct = processed_data.exposure_pct;

    if (!validate_position_data(account_snapshot.pos_details)) {
        return false;
    }

    return true;
}

bool MarketDataValidator::validate_price_data(const Bar& bar_data) const {
    // Validate that price data is not null/empty
    if (std::isnan(bar_data.close_price) || std::isnan(bar_data.open_price) || std::isnan(bar_data.high_price) || std::isnan(bar_data.low_price) ||
        !std::isfinite(bar_data.close_price) || !std::isfinite(bar_data.open_price) || !std::isfinite(bar_data.high_price) || !std::isfinite(bar_data.low_price)) {
        return false;
    }

    if (bar_data.close_price <= 0.0 || bar_data.open_price <= 0.0 || bar_data.high_price <= 0.0 || bar_data.low_price <= 0.0) {
        return false;
    }

    // Validate OHLC data is reasonable (H >= L, H >= C, L <= C)
    if (bar_data.high_price < bar_data.low_price || bar_data.high_price < bar_data.close_price || bar_data.low_price > bar_data.close_price) {
        return false;
    }

    return true;
}

bool MarketDataValidator::validate_technical_indicators(const MarketSnapshot& market_snapshot) const {
    // ATR can be 0.0 when there are insufficient bars - allow it (ATR calculates continuously)
    // Only reject if ATR is invalid (NaN or infinite)
    if (std::isnan(market_snapshot.atr) || !std::isfinite(market_snapshot.atr)) {
        return false;
    }

    return true;
}

bool MarketDataValidator::validate_position_data(const PositionDetails& position_details) const {
    // Position details validation is generally permissive
    // Only validate for obvious errors
    if (std::isnan(position_details.current_value) || std::isnan(position_details.unrealized_pl)) {
        return false;
    }

    return true;
}

bool MarketDataValidator::is_quote_data_fresh_and_valid(const QuoteData& quote_data) const {
    if (quote_data.mid_price <= 0.0 || quote_data.timestamp.empty()) {
        return false;
    }
    
    try {
        // Parse timestamp to check freshness
        std::tm timestamp_parsed = {};
        std::istringstream timestamp_stream(quote_data.timestamp);
        timestamp_stream >> std::get_time(&timestamp_parsed, "%Y-%m-%dT%H:%M:%S");
        
        if (timestamp_stream.fail()) {
            return false;
        }
        
        auto quote_timestamp = std::mktime(&timestamp_parsed);
        auto current_timestamp = std::time(nullptr);
        auto quote_age_seconds = current_timestamp - quote_timestamp;
        
        bool is_quote_fresh = (quote_age_seconds < config.timing.quote_data_freshness_threshold_seconds);
        
        return is_quote_fresh;
    } catch (const std::exception& exception) {
        return false;
    }
}

} // namespace Core
} // namespace AlpacaTrader
