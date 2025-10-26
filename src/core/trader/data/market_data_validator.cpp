#include "market_data_validator.hpp"
#include "core/logging/market_data_logs.hpp"
#include <cmath>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::MarketDataLogs;

MarketDataValidator::MarketDataValidator(const SystemConfig& cfg) : config(cfg) {}

bool MarketDataValidator::validate_market_snapshot(const MarketSnapshot& market_snapshot) const {
    // Check if this is a default/empty MarketSnapshot (no data available)
    if (market_snapshot.atr == 0.0 && market_snapshot.avg_atr == 0.0 && market_snapshot.avg_vol == 0.0 &&
        market_snapshot.curr.o == 0.0 && market_snapshot.curr.h == 0.0 && market_snapshot.curr.l == 0.0 && market_snapshot.curr.c == 0.0) {
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
    if (std::isnan(bar_data.c) || std::isnan(bar_data.o) || std::isnan(bar_data.h) || std::isnan(bar_data.l) ||
        !std::isfinite(bar_data.c) || !std::isfinite(bar_data.o) || !std::isfinite(bar_data.h) || !std::isfinite(bar_data.l)) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Invalid Data",
            "NaN or infinite values detected in price data",
            0,
            config.logging.log_file
        );
        return false;
    }

    if (bar_data.c <= 0.0 || bar_data.o <= 0.0 || bar_data.h <= 0.0 || bar_data.l <= 0.0) {
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
    if (bar_data.h < bar_data.l || bar_data.h < bar_data.c || bar_data.l > bar_data.c) {
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

} // namespace Core
} // namespace AlpacaTrader
