#ifndef STARTUP_LOGS_HPP
#define STARTUP_LOGS_HPP

#include "configs/system_config.hpp"
#include "core/trader/data/account_manager.hpp"
#include "core/logging/async_logger.hpp"

using AlpacaTrader::Config::SystemConfig;

/**
 * Specialized logging for application startup sequence.
 * Handles all startup-related logging in a consistent format.
 */
class StartupLogs {
public:
    
    // Application header and branding
    static void log_application_header();
    static void log_api_endpoints_table(const AlpacaTrader::Config::SystemConfig& config);
    
    // Account status display
    static void log_account_overview(const AlpacaTrader::Core::AccountManager& account_manager);
    static void log_financial_summary(const AlpacaTrader::Core::AccountManager& account_manager);
    static void log_current_positions(const AlpacaTrader::Core::AccountManager& account_manager, const AlpacaTrader::Config::SystemConfig& config);
    
    // Data source configuration
    static void log_data_source_configuration(const AlpacaTrader::Config::SystemConfig& config);
    
    // Thread system startup
    static void log_thread_system_startup(const SystemConfig& config);
    
    
    // System configuration tables
    static void log_runtime_configuration(const AlpacaTrader::Config::SystemConfig& config);
    static void log_strategy_configuration(const AlpacaTrader::Config::SystemConfig& config);
    
private:
    static std::string format_currency(double amount);
};

#endif // STARTUP_LOGS_HPP

