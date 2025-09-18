#ifndef STARTUP_LOGGER_HPP
#define STARTUP_LOGGER_HPP

#include "../configs/system_config.hpp"
#include "../data/account_manager.hpp"

/**
 * Specialized logging for application startup sequence.
 * Handles all startup-related logging in a consistent format.
 */
class StartupLogger {
public:
    // Application header and branding
    static void log_application_header();
    
    // Account status display
    static void log_account_status_header();
    static void log_account_overview(const AccountManager& account_manager);
    static void log_financial_summary(const AccountManager& account_manager);
    static void log_current_positions(const AccountManager& account_manager);
    static void log_account_status_footer();
    
    // Data source configuration
    static void log_data_source_configuration(const SystemConfig& config);
    
    // Thread system startup
    static void log_thread_system_startup(const TimingConfig& timing_config);
    static void log_thread_started(const std::string& thread_name, const std::string& thread_info);
    static void log_thread_priority_status(const std::string& thread_name, const std::string& priority, bool success);
    static void log_thread_system_complete();
    
    // Trading system configuration
    static void log_trading_configuration(const TraderConfig& config);
    
private:
    static std::string format_currency(double amount);
};

#endif // STARTUP_LOGGER_HPP

