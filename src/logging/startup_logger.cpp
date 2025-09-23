#include "startup_logger.hpp"
#include "trading_logger.hpp"
#include "async_logger.hpp"
#include "logging_macros.hpp"
#include "../configs/timing_config.hpp"
#include "../threads/thread_manager.hpp"
#include "../utils/config_loader.hpp"
#include <iomanip>
#include <sstream>

std::string StartupLogger::format_currency(double amount) {
    std::ostringstream oss;
    oss << "$" << std::fixed << std::setprecision(2) << amount;
    return oss.str();
}

void StartupLogger::log_application_header() {
    log_message("", "");
    log_message("================================================================================", "");
    log_message("                                   ALPACA TRADER", "");
    log_message("                            Advanced Momentum Trading Bot", "");
    log_message("================================================================================", "");
    log_message("", "");
}


void StartupLogger::log_account_overview(const AccountManager& account_manager) {
    auto account_info = account_manager.get_account_info();
    
    TradingLogger::log_account_overview_table(
        account_info.account_number,
        account_info.status,
        account_info.currency,
        account_info.pattern_day_trader,
        account_info.created_at
    );
}

void StartupLogger::log_financial_summary(const AccountManager& account_manager) {
    auto account_info = account_manager.get_account_info();
    
    TradingLogger::log_financial_summary_table(
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

void StartupLogger::log_current_positions(const AccountManager& account_manager) {
    auto snapshot = account_manager.get_account_snapshot();
    
    TradingLogger::log_current_positions_table(
        snapshot.pos_details.qty,
        snapshot.pos_details.current_value,
        snapshot.pos_details.unrealized_pl,
        snapshot.exposure_pct,
        snapshot.open_orders
    );
}


void StartupLogger::log_data_source_configuration(const SystemConfig& config) {
    std::string account_type = (config.api.base_url.find("paper") != std::string::npos) ? "PAPER TRADING" : "LIVE TRADING";
    TradingLogger::log_data_source_table(config.target.symbol, account_type);
}

void StartupLogger::log_thread_system_startup(const TimingConfig& timing_config) {
    TradingLogger::log_thread_system_table(
        timing_config.thread_priorities.enable_thread_priorities,
        timing_config.thread_priorities.enable_cpu_affinity
    );
}



void StartupLogger::log_thread_system_complete() {
    // Get dynamic thread status data from ThreadSystem::Manager
    auto thread_status_data = ThreadSystem::Manager::get_thread_status_data();
    TradingLogger::log_thread_priorities_table(thread_status_data);
}


void StartupLogger::log_runtime_configuration(const SystemConfig& config) {
    TradingLogger::log_runtime_config_table(config);
}

void StartupLogger::log_strategy_configuration(const SystemConfig& config) {
    TradingLogger::log_strategy_config_table(config);
}

// =============================================================================
// APPLICATION INITIALIZATION
// =============================================================================

void StartupLogger::initialize_application_foundation(const SystemConfig& config, AsyncLogger& logger) {
    // Validate configuration
    std::string cfgError;
    if (!validate_config(config, cfgError)) {
        fprintf(stderr, "Config error: %s\n", cfgError.c_str());
        exit(1);
    }
    
    // Initialize global logger (logger already created with timestamped filename)
    initialize_global_logger(logger);
    set_log_thread_tag("MAIN  ");
    
    // Log application startup
    log_application_header();
}
