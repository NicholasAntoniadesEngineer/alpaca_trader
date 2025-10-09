#include "startup_logs.hpp"
#include "trading_logs.hpp"
#include "async_logger.hpp"
#include "logging_macros.hpp"
#include "configs/timing_config.hpp"
#include "configs/api_config.hpp"
#include "core/threads/thread_logic/thread_manager.hpp"
#include "core/trader/config_loader/config_loader.hpp"
#include <iomanip>
#include <sstream>

// Using declarations for cleaner code
using AlpacaTrader::Logging::log_message;
using AlpacaTrader::Logging::TradingLogs;
using AlpacaTrader::Core::AccountManager;
using AlpacaTrader::Logging::AsyncLogger;
using AlpacaTrader::Logging::set_log_thread_tag;

std::string StartupLogs::format_currency(double amount) {
    std::ostringstream oss;
    oss << "$" << std::fixed << std::setprecision(2) << amount;
    return oss.str();
}

void StartupLogs::log_application_header() {
    log_message("", "");
    log_message("================================================================================", "");
    log_message("                                   ALPACA TRADER", "");
    log_message("                            Advanced Momentum Trading Bot", "");
    log_message("================================================================================", "");
    log_message("", "");
}

void StartupLogs::log_api_endpoints_table(const AlpacaTrader::Config::SystemConfig& config) {
    using namespace AlpacaTrader::Config;
    
    log_message("┌─────────────────────────────────────────────────────────────────────────────┐", "");
    log_message("│                              API ENDPOINTS                                  │", "");
    log_message("├─────────────────────────────────────────────────────────────────────────────┤", "");
    log_message("│ Trading API (Paper)    │ " + config.api.base_url + "                   │", "");
    log_message("│ Market Data API        │ " + config.api.data_url + "                        │", "");
    log_message("├─────────────────────────────────────────────────────────────────────────────┤", "");
    log_message("│ GET " + config.api.endpoints.trading.account + "                       │ Account info (equity, buying power) │", "");
    log_message("│ GET " + config.api.endpoints.trading.positions + "                     │ All positions                       │", "");
    log_message("│ POST " + config.api.endpoints.trading.orders + "                       │ Place orders (market, bracket)      │", "");
    log_message("│ GET " + config.api.endpoints.trading.clock + "                         │ Market hours & status               │", "");
    log_message("│ GET " + config.api.endpoints.market_data.bars + "          │ Historical market data              │", "");
    log_message("│ GET " + config.api.endpoints.market_data.quotes_latest + " │ Real-time quotes                    │", "");
    log_message("└─────────────────────────────────────────────────────────────────────────────┘", "");
    log_message("", "");
}


void StartupLogs::log_account_overview(const AccountManager& account_manager) {
    auto [account_info, snapshot] = account_manager.fetch_account_data_bundled();
    
    TradingLogs::log_account_overview_table(
        account_info.account_number,
        account_info.status,
        account_info.currency,
        account_info.pattern_day_trader,
        account_info.created_at
    );
}

void StartupLogs::log_financial_summary(const AccountManager& account_manager) {
    auto [account_info, snapshot] = account_manager.fetch_account_data_bundled();
    
    TradingLogs::log_financial_summary_table(
        account_info.equity,
        account_info.last_equity,
        account_info.cash,
        account_info.buying_power,
        account_info.long_market_value,
        account_info.short_market_value,
        account_info.initial_margin,
        account_info.maintenance_margin,
        account_info.sma,
        static_cast<int>(account_info.day_trade_count),
        account_info.regt_buying_power,
        account_info.daytrading_buying_power
    );
}

void StartupLogs::log_current_positions(const AccountManager& account_manager) {
    auto [account_info, snapshot] = account_manager.fetch_account_data_bundled();
    
    TradingLogs::log_current_positions_table(
        snapshot.pos_details.qty,
        snapshot.pos_details.current_value,
        snapshot.pos_details.unrealized_pl,
        snapshot.exposure_pct,
        snapshot.open_orders
    );
}


void StartupLogs::log_data_source_configuration(const AlpacaTrader::Config::SystemConfig& config) {
    std::string account_type = (config.api.base_url.find("paper") != std::string::npos) ? "PAPER TRADING" : "LIVE TRADING";
    TradingLogs::log_data_source_table(config.strategy.symbol, account_type);
}

void StartupLogs::log_thread_system_startup(const SystemConfig& config) {
    TradingLogs::log_thread_system_table(
        true,  // Thread priorities are always enabled
        config.thread_registry.get_thread_settings("main").use_cpu_affinity
    );
}


void StartupLogs::log_runtime_configuration(const AlpacaTrader::Config::SystemConfig& config) {
    TradingLogs::log_runtime_config_table(config);
}

void StartupLogs::log_strategy_configuration(const AlpacaTrader::Config::SystemConfig& config) {
    TradingLogs::log_strategy_config_table(config);
}

// =============================================================================
// APPLICATION INITIALIZATION
// =============================================================================

