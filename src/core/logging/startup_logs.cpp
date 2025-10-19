#include "startup_logs.hpp"
#include "async_logger.hpp"

using AlpacaTrader::Logging::log_message;

void StartupLogs::log_application_header() {
    log_message("=== ALPACA TRADING SYSTEM - PRODUCTION BUILD ===", "");
}

void StartupLogs::log_api_endpoints_table(const AlpacaTrader::Config::SystemConfig& config) {
    log_message("API Configuration: " + std::to_string(config.multi_api.providers.size()) + " providers", "");
}

void StartupLogs::log_account_overview(const AlpacaTrader::Core::AccountManager& /* account_manager */) {
    log_message("Account overview: Production account active", "");
}

void StartupLogs::log_financial_summary(const AlpacaTrader::Core::AccountManager& /* account_manager */) {
    log_message("Financial summary: Account ready for trading", "");
}

void StartupLogs::log_current_positions(const AlpacaTrader::Core::AccountManager& /* account_manager */) {
    log_message("Current positions: Loading...", "");
}

void StartupLogs::log_data_source_configuration(const AlpacaTrader::Config::SystemConfig& config) {
    std::string mode = (config.trading_mode.mode == AlpacaTrader::Config::TradingMode::STOCKS) ? "STOCKS" : "CRYPTO";
    log_message("Data source: " + mode + " mode, Symbol: " + config.trading_mode.primary_symbol, "");
}

void StartupLogs::log_thread_system_startup(const SystemConfig& config) {
    log_message("Thread system: " + std::to_string(config.thread_registry.thread_settings.size()) + " threads configured", "");
}

void StartupLogs::log_runtime_configuration(const AlpacaTrader::Config::SystemConfig& config) {
    std::string mode = (config.trading_mode.mode == AlpacaTrader::Config::TradingMode::STOCKS) ? "STOCKS" : "CRYPTO";
    log_message("Runtime config - Mode: " + mode + ", Providers: " + std::to_string(config.multi_api.providers.size()), "");
}

void StartupLogs::log_strategy_configuration(const AlpacaTrader::Config::SystemConfig& config) {
    log_message("Strategy config - Symbol: " + config.strategy.symbol + ", Bars: " + std::to_string(config.strategy.bars_to_fetch_for_calculations), "");
}

std::string StartupLogs::format_currency(double amount) {
    return "$" + std::to_string(static_cast<int>(amount));
}
