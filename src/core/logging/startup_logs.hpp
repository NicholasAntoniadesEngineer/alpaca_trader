#ifndef STARTUP_LOGS_HPP
#define STARTUP_LOGS_HPP

#include "configs/system_config.hpp"
#include "core/trader/data/account_manager.hpp"
#include "core/logging/async_logger.hpp"

/**
 * Specialized logging for application startup sequence.
 * Handles all startup-related logging in a consistent format.
 */
class StartupLogs {
public:
    
    // Application header and branding
    static void log_application_header();
    static void log_api_endpoints_table(const std::string& base_url, const std::string& data_url, const std::string& orders_endpoint);
    
    // Account status display
    static void log_account_overview(const AlpacaTrader::Core::AccountManager& account_manager);
    static void log_financial_summary(const AlpacaTrader::Core::AccountManager& account_manager);
    static void log_current_positions(const AlpacaTrader::Core::AccountManager& account_manager);
    
    // Data source configuration
    static void log_data_source_configuration(const AlpacaTrader::Config::SystemConfig& config);
    
    // Thread system startup
    static void log_thread_system_startup(const TimingConfig& timing_config);
    
    
    // System configuration tables
    static void log_runtime_configuration(const AlpacaTrader::Config::SystemConfig& config);
    static void log_strategy_configuration(const AlpacaTrader::Config::SystemConfig& config);
    
private:
    static std::string format_currency(double amount);
};

#endif // STARTUP_LOGS_HPP

