// main.cpp
#include "trader/config_loader/config_loader.hpp"
#include "configs/system_config.hpp"
#include "system/system_state.hpp"
#include "system/system_threads.hpp"
#include "system/system_manager.hpp"
#include "logging/logger/async_logger.hpp"


// =============================================================================
// MAIN APPLICATION ENTRY POINT
// =============================================================================

int main() {
    try {
        // Initialize minimal logging context early - required before any logging calls
        auto early_logging_context = std::make_shared<AlpacaTrader::Logging::LoggingContext>();
        AlpacaTrader::Logging::set_logging_context(*early_logging_context);
        
        // Load system configuration (may call log_message during loading)
        AlpacaTrader::Config::SystemConfig initial_config;
        if (int result = load_system_config(initial_config)) {
            AlpacaTrader::Logging::log_message(std::string("ERROR: Config load failed with result: ") + std::to_string(result), "");
            return result;
        }
        
        // Create system state
        SystemState system_state(initial_config);
        
        // Transfer logging context to system state
        system_state.logging_context = early_logging_context;
        
        // Initialize application foundation (logging, validation)
        auto logger = AlpacaTrader::Logging::initialize_application_foundation(system_state.config);

        // Initialize CSV logging for bars and trades (stored in logging context)
        (void)AlpacaTrader::Logging::initialize_csv_bars_logger("bars_logs");
        (void)AlpacaTrader::Logging::initialize_csv_trade_logger("trade_logs");

        // Start the complete trading system
        SystemThreads thread_handles = SystemManager::startup(system_state, logger);
        
        // Run until shutdown signal
        SystemManager::run(system_state);
        
        // Clean shutdown
        SystemManager::shutdown(system_state, logger);
        return 0;
    } catch (const std::exception& exception_error) {
        AlpacaTrader::Logging::log_message(std::string("FATAL: ") + exception_error.what(), "");
        return 1;
    } catch (...) {
        AlpacaTrader::Logging::log_message("FATAL: Unknown fatal error", "");
        return 1;
    }
}
