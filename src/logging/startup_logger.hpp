#ifndef STARTUP_LOGGER_HPP
#define STARTUP_LOGGER_HPP

#include "configs/system_config.hpp"
#include "core/account_manager.hpp"
#include "logging/async_logger.hpp"

/**
 * Specialized logging for application startup sequence.
 * Handles all startup-related logging in a consistent format.
 */
class StartupLogger {
public:
    // Application initialization
    static std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> initialize_application_foundation(const SystemConfig& config);
    
    // Application header and branding
    static void log_application_header();
    
    // Account status display
    static void log_account_overview(const AlpacaTrader::Core::AccountManager& account_manager);
    static void log_financial_summary(const AlpacaTrader::Core::AccountManager& account_manager);
    static void log_current_positions(const AlpacaTrader::Core::AccountManager& account_manager);
    
    // Data source configuration
    static void log_data_source_configuration(const SystemConfig& config);
    
    // Thread system startup
    static void log_thread_system_startup(const TimingConfig& timing_config);
    static void log_thread_system_complete();
    
    
    // System configuration tables
    static void log_runtime_configuration(const SystemConfig& config);
    static void log_strategy_configuration(const SystemConfig& config);
    
private:
    static std::string format_currency(double amount);
};

#endif // STARTUP_LOGGER_HPP

