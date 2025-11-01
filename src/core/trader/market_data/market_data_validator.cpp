#include "market_data_validator.hpp"
#include "core/logging/logs/market_data_logs.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::MarketDataLogs;

MarketDataValidator::MarketDataValidator(const SystemConfig& cfg) : config(cfg) {}

bool MarketDataValidator::validate_market_snapshot(const MarketSnapshot& market_snapshot) const {
    // Check if this is a default/empty MarketSnapshot (no data available)
    if (market_snapshot.atr == 0.0 && market_snapshot.avg_atr == 0.0 && market_snapshot.avg_vol == 0.0 &&
        market_snapshot.curr.open_price == 0.0 && market_snapshot.curr.high_price == 0.0 && market_snapshot.curr.low_price == 0.0 && market_snapshot.curr.close_price == 0.0) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "No Data Available",
            "Symbol may not exist or market is closed",
            0,
            config.logging.log_file
        );
        return false;
    }

    // Validate price data
    if (!validate_price_data(market_snapshot.curr)) {
        return false;
    }

    // Validate technical indicators
    if (!validate_technical_indicators(market_snapshot)) {
        return false;
    }

    return true;
}

bool MarketDataValidator::validate_account_snapshot(const AccountSnapshot& account_snapshot) const {
    // Validate equity
    if (account_snapshot.equity <= 0.0) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Invalid Account Data",
            "Account equity is zero or negative",
            0,
            config.logging.log_file
        );
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
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Invalid Data",
            "NaN or infinite values detected in price data",
            0,
            config.logging.log_file
        );
        return false;
    }

    if (bar_data.close_price <= 0.0 || bar_data.open_price <= 0.0 || bar_data.high_price <= 0.0 || bar_data.low_price <= 0.0) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Invalid Data",
            "Price is zero or negative",
            0,
            config.logging.log_file
        );
        return false;
    }

    // Validate OHLC data is reasonable (H >= L, H >= C, L <= C)
    if (bar_data.high_price < bar_data.low_price || bar_data.high_price < bar_data.close_price || bar_data.low_price > bar_data.close_price) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Invalid Data",
            "OHLC relationship violation - invalid price data structure",
            0,
            config.logging.log_file
        );
        return false;
    }

    return true;
}

bool MarketDataValidator::validate_technical_indicators(const MarketSnapshot& market_snapshot) const {
    if (market_snapshot.atr <= 0.0) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Insufficient Data",
            "ATR is zero or negative - insufficient volatility data for trading",
            0,
            config.logging.log_file
        );
        return false;
    }

    if (std::isnan(market_snapshot.atr) || !std::isfinite(market_snapshot.atr)) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Invalid Data",
            "ATR contains NaN or infinite values",
            0,
            config.logging.log_file
        );
        return false;
    }

    return true;
}

bool MarketDataValidator::validate_position_data(const PositionDetails& position_details) const {
    // Position details validation is generally permissive
    // Only validate for obvious errors
    if (std::isnan(position_details.current_value) || std::isnan(position_details.unrealized_pl)) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Invalid Data",
            "Position data contains NaN values",
            0,
            config.logging.log_file
        );
        return false;
    }

    return true;
}

bool MarketDataValidator::is_quote_data_fresh_and_valid(const QuoteData& quote_data) const {
    if (quote_data.mid_price <= 0.0 || quote_data.timestamp.empty()) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Invalid Quote Data",
            "Quote data is missing or invalid",
            0,
            config.logging.log_file
        );
        return false;
    }
    
    try {
        // Parse timestamp to check freshness
        std::tm timestamp_parsed = {};
        std::istringstream timestamp_stream(quote_data.timestamp);
        timestamp_stream >> std::get_time(&timestamp_parsed, "%Y-%m-%dT%H:%M:%S");
        
        if (timestamp_stream.fail()) {
            MarketDataLogs::log_market_data_failure_summary(
                config.trading_mode.primary_symbol,
                "Invalid Quote Data",
                "Unable to parse quote timestamp",
                0,
                config.logging.log_file
            );
            return false;
        }
        
        auto quote_timestamp = std::mktime(&timestamp_parsed);
        auto current_timestamp = std::time(nullptr);
        auto quote_age_seconds = current_timestamp - quote_timestamp;
        
        bool is_quote_fresh = (quote_age_seconds < config.timing.quote_data_freshness_threshold_seconds);
        
        if (!is_quote_fresh) {
            MarketDataLogs::log_market_data_failure_summary(
                config.trading_mode.primary_symbol,
                "Stale Quote Data",
                "Quote data is stale (age: " + std::to_string(quote_age_seconds) + "s)",
                quote_age_seconds,
                config.logging.log_file
            );
        }
        
        return is_quote_fresh;
    } catch (const std::exception& exception) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Quote Data Error",
            "Error parsing quote timestamp: " + std::string(exception.what()),
            0,
            config.logging.log_file
        );
        return false;
    }
}

} // namespace Core
} // namespace AlpacaTrader
