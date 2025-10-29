#include "startup_logs.hpp"
#include "core/logging/logger/async_logger.hpp"
#include <iomanip>
#include <sstream>

using AlpacaTrader::Logging::log_message;

void StartupLogs::log_application_header() {
    log_message("", "");
    log_message("================================================================================", "");
    log_message("                                 ALPACA TRADER", "");
    log_message("                          Advanced Momentum Trading Bot", "");
    log_message("================================================================================", "");
    log_message("", "");
}

void StartupLogs::log_api_endpoints_table(const AlpacaTrader::Config::SystemConfig& config) {
    log_message("┌─────────────────────────────────────────────────────────────────────────────┐", "");
    log_message("│                              API CONFIGURATION                              │", "");
    log_message("├─────────────────────────────────────────────────────────────────────────────┤", "");
    
    auto& providers = config.multi_api.providers;
    
    if (providers.find(AlpacaTrader::Config::ApiProvider::ALPACA_TRADING) != providers.end()) {
        const auto& alpaca_trading = providers.at(AlpacaTrader::Config::ApiProvider::ALPACA_TRADING);
        log_message("│ ALPACA TRADING API                                                          │", "");
        std::string url_line = "│ Trading & Orders   │ " + alpaca_trading.base_url;
        url_line += std::string(82 - url_line.length(), ' ') + "│";
        log_message(url_line, "");
    }
    
    if (providers.find(AlpacaTrader::Config::ApiProvider::ALPACA_STOCKS) != providers.end()) {
        const auto& alpaca_stocks = providers.at(AlpacaTrader::Config::ApiProvider::ALPACA_STOCKS);
        log_message("│ ALPACA STOCKS API                                                           │", "");
        std::string url_line = "│ Market Data        │ " + alpaca_stocks.base_url;
        url_line += std::string(82 - url_line.length(), ' ') + "│";
        log_message(url_line, "");
    }
    
    if (providers.find(AlpacaTrader::Config::ApiProvider::POLYGON_CRYPTO) != providers.end()) {
        const auto& polygon_crypto = providers.at(AlpacaTrader::Config::ApiProvider::POLYGON_CRYPTO);
        log_message("│ POLYGON CRYPTO API                                                          │", "");
        std::string url_line = "│ Crypto Data        │ " + polygon_crypto.base_url;
        url_line += std::string(82 - url_line.length(), ' ') + "│";
        log_message(url_line, "");
    }
    
    log_message("├─────────────────────────────────────────────────────────────────────────────┤", "");
    
    if (providers.find(AlpacaTrader::Config::ApiProvider::ALPACA_TRADING) != providers.end()) {
        const auto& endpoints = providers.at(AlpacaTrader::Config::ApiProvider::ALPACA_TRADING).endpoints;
        if (!endpoints.account.empty()) {
            std::string line = "│ GET " + endpoints.account;
            line += std::string(35 - endpoints.account.length(), ' ') + "│ Account info (equity, buying power)│";
            log_message(line, "");
        }
        if (!endpoints.positions.empty()) {
            std::string line = "│ GET " + endpoints.positions;
            line += std::string(35 - endpoints.positions.length(), ' ') + "│ All positions                      │";
            log_message(line, "");
        }
        if (!endpoints.orders.empty()) {
            std::string line = "│ POST " + endpoints.orders;
            line += std::string(34 - endpoints.orders.length(), ' ') + "│ Place orders (market, bracket)     │";
            log_message(line, "");
        }
        if (!endpoints.clock.empty()) {
            std::string line = "│ GET " + endpoints.clock;
            line += std::string(35 - endpoints.clock.length(), ' ') + "│ Market hours & status              │";
            log_message(line, "");
        }
        if (!endpoints.bars.empty()) {
            std::string line = "│ GET " + endpoints.bars;
            line += std::string(35 - endpoints.bars.length(), ' ') + "│ Historical market data             │";
            log_message(line, "");
        }
        if (!endpoints.quotes_latest.empty()) {
            std::string line = "│ GET " + endpoints.quotes_latest;
            line += std::string(35 - endpoints.quotes_latest.length(), ' ') + "│ Real-time quotes                   │";
            log_message(line, "");
        }
    }
    
    log_message("└─────────────────────────────────────────────────────────────────────────────┘", "");
    log_message("", "");
}

void StartupLogs::log_account_overview(const AlpacaTrader::Core::AccountManager& account_manager) {
    auto account_info = account_manager.fetch_account_info();
    
    log_message("┌───────────────────┬──────────────────────────────────────────────────┐", "");
    log_message("│ Account Overview  │ Details                                          │", "");
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    log_message("│ Account Number    │ " + account_info.account_number + std::string(49 - account_info.account_number.length(), ' ') + "│", "");
    log_message("│ Status            │ " + account_info.status + std::string(49 - account_info.status.length(), ' ') + "│", "");
    log_message("│ Currency          │ " + account_info.currency + std::string(49 - account_info.currency.length(), ' ') + "│", "");
    log_message("│ Pattern Day Trade │ " + std::string(account_info.pattern_day_trader ? "YES" : "NO") + std::string(46, ' ') + "│", "");
    log_message("│ Created           │ " + account_info.created_at + std::string(49 - account_info.created_at.length(), ' ') + "│", "");
    log_message("└───────────────────┴──────────────────────────────────────────────────┘", "");
}

void StartupLogs::log_financial_summary(const AlpacaTrader::Core::AccountManager& account_manager) {
    auto account_info = account_manager.fetch_account_info();
    
    log_message("┌───────────────────┬──────────────────────────────────────────────────┐", "");
    log_message("│ Financial Summary │ Account Values                                   │", "");
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    log_message("│ Equity            │ " + format_currency(account_info.equity) + std::string(49 - format_currency(account_info.equity).length(), ' ') + "│", "");
    log_message("│ Last Equity       │ " + format_currency(account_info.last_equity) + std::string(49 - format_currency(account_info.last_equity).length(), ' ') + "│", "");
    log_message("│ Cash              │ " + format_currency(account_info.cash) + std::string(49 - format_currency(account_info.cash).length(), ' ') + "│", "");
    log_message("│ Buying Power      │ " + format_currency(account_info.buying_power) + std::string(49 - format_currency(account_info.buying_power).length(), ' ') + "│", "");
    log_message("│ Long Market Val   │ " + format_currency(account_info.long_market_value) + std::string(49 - format_currency(account_info.long_market_value).length(), ' ') + "│", "");
    log_message("│ Short Market Val  │ " + format_currency(account_info.short_market_value) + std::string(49 - format_currency(account_info.short_market_value).length(), ' ') + "│", "");
    log_message("│ Initial Margin    │ " + format_currency(account_info.initial_margin) + std::string(49 - format_currency(account_info.initial_margin).length(), ' ') + "│", "");
    log_message("│ Maint Margin      │ " + format_currency(account_info.maintenance_margin) + std::string(49 - format_currency(account_info.maintenance_margin).length(), ' ') + "│", "");
    log_message("│ SMA               │ " + format_currency(account_info.sma) + std::string(49 - format_currency(account_info.sma).length(), ' ') + "│", "");
    log_message("│ Day Trade Count   │ " + std::to_string(static_cast<int>(account_info.day_trade_count)) + std::string(49 - std::to_string(static_cast<int>(account_info.day_trade_count)).length(), ' ') + "│", "");
    log_message("│ RegT Buying Power │ " + format_currency(account_info.regt_buying_power) + std::string(49 - format_currency(account_info.regt_buying_power).length(), ' ') + "│", "");
    log_message("│ DT Buying Power   │ " + format_currency(account_info.daytrading_buying_power) + std::string(49 - format_currency(account_info.daytrading_buying_power).length(), ' ') + "│", "");
    log_message("└───────────────────┴──────────────────────────────────────────────────┘", "");
}

void StartupLogs::log_current_positions(const AlpacaTrader::Core::AccountManager& account_manager, const AlpacaTrader::Config::SystemConfig& config) {
    AlpacaTrader::Core::SymbolRequest sym_req{config.trading_mode.primary_symbol};
    auto position = account_manager.fetch_position_details(sym_req);
    int open_orders = account_manager.fetch_open_orders_count(sym_req);
    
    log_message("┌───────────────────┬──────────────────────────────────────────────────┐", "");
    log_message("│ Current Position  │ Portfolio Status                                 │", "");
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    
    if (position.position_quantity == 0) {
        log_message("│ Position          │ No position" + std::string(38, ' ') + "│", "");
        log_message("│ Current Value     │ $0.00" + std::string(44, ' ') + "│", "");
        log_message("│ Unrealized P/L    │ $0.00" + std::string(44, ' ') + "│", "");
        log_message("│ Exposure          │ 0.00%" + std::string(44, ' ') + "│", "");
    } else {
        log_message("│ Symbol            │ " + config.trading_mode.primary_symbol + std::string(50 - config.trading_mode.primary_symbol.length(), ' ') + "│", "");
        log_message("│ Quantity          │ " + std::to_string(position.position_quantity) + std::string(50 - std::to_string(position.position_quantity).length(), ' ') + "│", "");
        log_message("│ Current Value     │ " + format_currency(position.current_value) + std::string(50 - format_currency(position.current_value).length(), ' ') + "│", "");
        log_message("│ Unrealized P/L    │ " + format_currency(position.unrealized_pl) + std::string(50 - format_currency(position.unrealized_pl).length(), ' ') + "│", "");
    }
    
    log_message("│ Open Orders       │ " + std::to_string(open_orders) + std::string(49 - std::to_string(open_orders).length(), ' ') + "│", "");
    log_message("└───────────────────┴──────────────────────────────────────────────────┘", "");
}

void StartupLogs::log_data_source_configuration(const AlpacaTrader::Config::SystemConfig& config) {
    std::string mode = (config.trading_mode.mode == AlpacaTrader::Config::TradingMode::STOCKS) ? "STOCKS" : "CRYPTO";
    
    log_message("┌───────────────────┬──────────────────────────────────────────────────┐", "");
    log_message("│ Data Sources      │ Multi-API Configuration                          │", "");
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    log_message("│ Trading Mode      │ " + mode + std::string(49 - mode.length(), ' ') + "│", "");
    log_message("│ Trading Symbol    │ " + config.trading_mode.primary_symbol + std::string(49 - config.trading_mode.primary_symbol.length(), ' ') + "│", "");
    log_message("│ Account Type      │ PAPER TRADING" + std::string(36, ' ') + "│", "");
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    
    auto& providers = config.multi_api.providers;
    
    if (providers.find(AlpacaTrader::Config::ApiProvider::ALPACA_TRADING) != providers.end()) {
        log_message("│ Alpaca Trading    │ Orders, positions, account management            │", "");
    }
    
    if (config.trading_mode.mode == AlpacaTrader::Config::TradingMode::STOCKS) {
        if (providers.find(AlpacaTrader::Config::ApiProvider::ALPACA_STOCKS) != providers.end()) {
            log_message("│ Alpaca Stocks     │ Market data (IEX feed, 15-min delay)             │", "");
        }
        log_message("│ Active Provider   │ Alpaca Trading + Alpaca Stocks                   │", "");
    } else {
        if (providers.find(AlpacaTrader::Config::ApiProvider::POLYGON_CRYPTO) != providers.end()) {
            log_message("│ Polygon Crypto    │ Real-time crypto market data                     │", "");
        }
        log_message("│ Active Provider   │ Alpaca Trading + Polygon Crypto                  │", "");
    }
    
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    log_message("│ Total Configured  │ " + std::to_string(config.multi_api.providers.size()) + " API providers available" + std::string(27 - std::to_string(config.multi_api.providers.size()).length(), ' ') + "│", "");
    log_message("└───────────────────┴──────────────────────────────────────────────────┘", "");
}

void StartupLogs::log_thread_system_startup(const SystemConfig& config) {
    log_message("┌───────────────────┬──────────────────────────────────────────────────┐", "");
    log_message("│ Thread System     │ Performance Settings                             │", "");
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    log_message("│ Total Threads     │ " + std::to_string(config.thread_registry.thread_settings.size()) + " configured" + std::string(38 - std::to_string(config.thread_registry.thread_settings.size()).length(), ' ') + "│", "");
    log_message("│ Thread Priorities │ ENABLED" + std::string(42, ' ') + "│", "");
    log_message("│ CPU Affinity      │ ENABLED" + std::string(42, ' ') + "│", "");
    log_message("└───────────────────┴──────────────────────────────────────────────────┘", "");
}

void StartupLogs::log_runtime_configuration(const AlpacaTrader::Config::SystemConfig& config) {
    log_message("┌───────────────────┬──────────────────────────────────────────────────┐", "");
    log_message("│ Runtime Config    │ System Settings                                  │", "");
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    log_message("│ Environment       │ PAPER" + std::string(44, ' ') + "│", "");
    if (!config.multi_api.providers.empty()) {
        const auto& first_provider = config.multi_api.providers.begin()->second;
        log_message("│ API Version       │ " + first_provider.api_version + std::string(49 - first_provider.api_version.length(), ' ') + "│", "");
        log_message("│ Retry Count       │ " + std::to_string(first_provider.retry_count) + std::string(49 - std::to_string(first_provider.retry_count).length(), ' ') + "│", "");
        log_message("│ Timeout           │ " + std::to_string(first_provider.timeout_seconds) + "s" + std::string(48 - std::to_string(first_provider.timeout_seconds).length(), ' ') + "│", "");
    }
    
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    
    std::stringstream exp_ss;
    exp_ss << std::fixed << std::setprecision(0) << (config.strategy.max_account_exposure_percentage * 100) << "%";
    log_message("│ Max Exposure      │ " + exp_ss.str() + std::string(49 - exp_ss.str().length(), ' ') + "│", "");
    
    std::stringstream bp_ss;
    bp_ss << std::fixed << std::setprecision(2) << config.strategy.buying_power_utilization_percentage;
    log_message("│ BP Usage Factor   │ " + bp_ss.str() + std::string(49 - bp_ss.str().length(), ' ') + "│", "");
    
    std::stringstream loss_ss;
    loss_ss << std::fixed << std::setprecision(6) << (config.strategy.max_daily_loss_percentage * 100) << "%";
    log_message("│ Daily Max Loss    │ " + loss_ss.str() + std::string(49 - loss_ss.str().length(), ' ') + "│", "");
    
    std::stringstream profit_ss;
    profit_ss << std::fixed << std::setprecision(6) << (config.strategy.daily_profit_target_percentage * 100) << "%";
    log_message("│ Profit Target     │ " + profit_ss.str() + std::string(49 - profit_ss.str().length(), ' ') + "│", "");
    
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    
    log_message("│ Historical Bars F │ " + std::to_string(config.timing.historical_data_fetch_period_minutes) + "m" + std::string(48 - std::to_string(config.timing.historical_data_fetch_period_minutes).length(), ' ') + "│", "");
    
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    
    log_message("│ Wash Trade Preven │ " + std::string(config.timing.enable_wash_trade_prevention_mechanism ? "Enabled" : "Disabled") + std::string(49 - std::string(config.timing.enable_wash_trade_prevention_mechanism ? "Enabled" : "Disabled").length(), ' ') + "│", "");
    log_message("│ Min Order Interva │ " + std::to_string(config.timing.minimum_interval_between_orders_seconds) + " seconds" + std::string(41 - std::to_string(config.timing.minimum_interval_between_orders_seconds).length(), ' ') + "│", "");
    
    log_message("└───────────────────┴──────────────────────────────────────────────────┘", "");
}

void StartupLogs::log_strategy_configuration(const AlpacaTrader::Config::SystemConfig& config) {
    log_message("┌───────────────────┬──────────────────────────────────────────────────┐", "");
    log_message("│ Strategy Config   │ Trading Strategy Settings                        │", "");
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    
    log_message("│ Buy Equal Close   │ " + std::string(config.strategy.buy_signals_allow_equal_close ? "YES" : "NO") + std::string(49 - std::string(config.strategy.buy_signals_allow_equal_close ? "YES" : "NO").length(), ' ') + "│", "");
    log_message("│ Buy Higher High   │ " + std::string(config.strategy.buy_signals_require_higher_high ? "YES" : "NO") + std::string(49 - std::string(config.strategy.buy_signals_require_higher_high ? "YES" : "NO").length(), ' ') + "│", "");
    log_message("│ Buy Higher Low    │ " + std::string(config.strategy.buy_signals_require_higher_low ? "YES" : "NO") + std::string(49 - std::string(config.strategy.buy_signals_require_higher_low ? "YES" : "NO").length(), ' ') + "│", "");
    
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    
    log_message("│ Sell Equal Close  │ " + std::string(config.strategy.sell_signals_allow_equal_close ? "YES" : "NO") + std::string(49 - std::string(config.strategy.sell_signals_allow_equal_close ? "YES" : "NO").length(), ' ') + "│", "");
    log_message("│ Sell Lower Low    │ " + std::string(config.strategy.sell_signals_require_lower_low ? "YES" : "NO") + std::string(49 - std::string(config.strategy.sell_signals_require_lower_low ? "YES" : "NO").length(), ' ') + "│", "");
    log_message("│ Sell Lower High   │ " + std::string(config.strategy.sell_signals_require_lower_high ? "YES" : "NO") + std::string(   49 - std::string(config.strategy.sell_signals_require_lower_high ? "YES" : "NO").length(), ' ') + "│", "");
    
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    
    std::stringstream atr_ss;
    atr_ss << std::fixed << std::setprecision(2) << config.strategy.entry_signal_atr_multiplier;
    log_message("│ ATR Multiplier    │ " + atr_ss.str() + std::string(49 - atr_ss.str().length(), ' ') + "│", "");
    
    std::stringstream vol_ss;
    vol_ss << std::fixed << std::setprecision(2) << config.strategy.entry_signal_volume_multiplier;
    log_message("│ Volume Multiplier │ " + vol_ss.str() + std::string(49 - vol_ss.str().length(), ' ') + "│", "");
    
    log_message("│ ATR Period        │ " + std::to_string(config.strategy.atr_calculation_bars) + std::string(49 - std::to_string(config.strategy.atr_calculation_bars).length(), ' ') + "│", "");
    log_message("│ Avg ATR Multi     │ " + std::to_string(config.strategy.average_atr_comparison_multiplier) + std::string(49 - std::to_string(config.strategy.average_atr_comparison_multiplier).length(), ' ') + "│", "");
    
    log_message("├───────────────────┼──────────────────────────────────────────────────┤", "");
    
    std::stringstream risk_ss;
    risk_ss << std::fixed << std::setprecision(2) << (config.strategy.risk_percentage_per_trade * 100) << "%";
    log_message("│ Risk per Trade    │ " + risk_ss.str() + std::string(49 - risk_ss.str().length(), ' ') + "│", "");
    
    log_message("│ Max Trade Value   │ $" + std::to_string(static_cast<int>(config.strategy.maximum_dollar_value_per_trade)) + std::string(48 - std::to_string(static_cast<int>(config.strategy.maximum_dollar_value_per_trade)).length(), ' ') + "│", "");
    
    std::stringstream rr_ss;
    rr_ss << "1:" << std::fixed << std::setprecision(2) << config.strategy.rr_ratio;
    log_message("│ RR Ratio          │ " + rr_ss.str() + std::string(49 - rr_ss.str().length(), ' ') + "│", "");
    
    std::string tp_method = config.strategy.use_take_profit_percentage ? "Percentage" : "ATR-based";
    std::stringstream tp_ss;
    tp_ss << tp_method << " (" << std::fixed << std::setprecision(2) << (config.strategy.take_profit_percentage * 100) << "%)";
    log_message("│ Take Profit Metho │ " + tp_ss.str() + std::string(49 - tp_ss.str().length(), ' ') + "│", "");
    
    std::string fixed_shares = config.strategy.enable_fixed_share_quantity_per_trade ? 
        "Enabled (" + std::to_string(config.strategy.fixed_share_quantity_per_trade) + " shares)" : "Disabled";
    log_message("│ Fixed Shares      │ " + fixed_shares + std::string(49 - fixed_shares.length(), ' ') + "│", "");
    
    std::string pos_mult = config.strategy.enable_risk_based_position_multiplier ? "Enabled" : "Disabled";
    log_message("│ Position Multipli │ " + pos_mult + std::string(49 - pos_mult.length(), ' ') + "│", "");
    log_message("│ Multi Positions   │ " + std::string(config.strategy.allow_multiple_positions_per_symbol ? "YES" : "NO") + std::string(49 - std::string(config.strategy.allow_multiple_positions_per_symbol ? "YES" : "NO").length(), ' ') + "│", "");
    log_message("│ Max Layers        │ " + std::to_string(config.strategy.maximum_position_layers) + std::string(49 - std::to_string(config.strategy.maximum_position_layers).length(), ' ') + "│", "");
    log_message("│ Close on Reverse  │ " + std::string(config.strategy.close_positions_on_signal_reversal ? "YES" : "NO") + std::string(49 - std::string(config.strategy.close_positions_on_signal_reversal ? "YES" : "NO").length(), ' ') + "│", "");
    
    log_message("└───────────────────┴──────────────────────────────────────────────────┘", "");
}

std::string StartupLogs::format_currency(double amount) {
    std::stringstream ss;
    ss << "$" << std::fixed << std::setprecision(2) << amount;
    return ss.str();
}
