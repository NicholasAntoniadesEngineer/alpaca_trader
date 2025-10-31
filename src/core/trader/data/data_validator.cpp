#include "data_validator.hpp"
#include "core/logging/logs/market_data_logs.hpp"
#include "api/general/api_manager.hpp"
#include <cmath>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::MarketDataLogs;

DataValidator::DataValidator(const SystemConfig& cfg) : config(cfg) {}

bool DataValidator::validate_market_data(const MarketSnapshot& market) const {
    // Check if this is a default/empty MarketSnapshot (no data available)
    if (market.atr == 0.0 && market.avg_atr == 0.0 && market.avg_vol == 0.0 &&
        market.curr.open_price == 0.0 && market.curr.high_price == 0.0 && market.curr.low_price == 0.0 && market.curr.close_price == 0.0) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "No Data Available",
            "Symbol may not exist or market is closed",
            0,
            config.logging.log_file
        );
        return false;
    }

    // Validate that market data is not null/empty
    if (std::isnan(market.curr.close_price) || std::isnan(market.atr) || !std::isfinite(market.curr.close_price) || !std::isfinite(market.atr)) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Invalid Data",
            "NaN or infinite values detected in market data",
            0,
            config.logging.log_file
        );
        return false;
    }

    if (market.curr.close_price <= 0.0) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Invalid Data",
            "Price is zero or negative",
            0,
            config.logging.log_file
        );
        return false;
    }

    if (market.atr <= 0.0) {
        MarketDataLogs::log_market_data_failure_summary(
            config.trading_mode.primary_symbol,
            "Insufficient Data",
            "ATR is zero or negative - insufficient volatility data for trading",
            0,
            config.logging.log_file
        );
        return false;
    }

    // Validate OHLC data is reasonable (H >= L, H >= C, L <= C)
    if (market.curr.high_price < market.curr.low_price || market.curr.high_price < market.curr.close_price || market.curr.low_price > market.curr.close_price) {
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

bool DataValidator::validate_sync_state_pointers(MarketDataSyncState& sync_state) const {
    if (!(sync_state.mtx && sync_state.cv && sync_state.market && sync_state.account && 
          sync_state.has_market && sync_state.has_account && sync_state.running && sync_state.allow_fetch)) {
        MarketDataLogs::log_sync_state_error("Invalid data sync configuration", config.logging.log_file);
        return false;
    }
    return true;
}

bool DataValidator::fetch_and_validate_market_bars(ProcessedData& data, API::ApiManager& api_manager, const std::string& symbol) const {
    try {
        BarRequest bar_request(symbol, config.strategy.bars_to_fetch_for_calculations);
        std::vector<Bar> bars = api_manager.get_recent_bars(bar_request);
        
        if (bars.empty()) {
            MarketDataLogs::log_market_data_failure_summary(
                symbol,
                "No Bars Received",
                "API returned empty bar data",
                0,
                config.logging.log_file
            );
            return false;
        }

        if (static_cast<int>(bars.size()) < config.strategy.bars_to_fetch_for_calculations) {
            MarketDataLogs::log_market_data_failure_summary(
                symbol,
                "Insufficient Bars",
                "Received " + std::to_string(bars.size()) + " bars, need " + std::to_string(config.strategy.bars_to_fetch_for_calculations),
                bars.size(),
                config.logging.log_file
            );
            return false;
        }

        // Validate individual bars
        for (const auto& bar : bars) {
            if (bar.close_price <= 0.0 || bar.high_price <= 0.0 || bar.low_price <= 0.0 || bar.open_price <= 0.0) {
                MarketDataLogs::log_market_data_failure_summary(
                    symbol,
                    "Invalid Bar Data",
                    "Bar contains zero or negative prices",
                    bars.size(),
                    config.logging.log_file
                );
                return false;
            }
            
            if (bar.high_price < bar.low_price || bar.high_price < bar.close_price || bar.low_price > bar.close_price) {
                MarketDataLogs::log_market_data_failure_summary(
                    symbol,
                    "Invalid Bar Data",
                    "Bar OHLC relationship violation",
                    bars.size(),
                    config.logging.log_file
                );
                return false;
            }
        }

        return true;
    } catch (const std::exception& validation_exception_error) {
        MarketDataLogs::log_market_data_failure_summary(
            symbol,
            "API Exception",
            validation_exception_error.what(),
            0,
            config.logging.log_file
        );
        return false;
    }
}

} // namespace Core
} // namespace AlpacaTrader
