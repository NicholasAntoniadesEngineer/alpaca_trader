// main.cpp
#include "core/trader/config_loader/config_loader.hpp"
#include "configs/system_config.hpp"
#include "core/system/system_state.hpp"
#include "core/system/system_threads.hpp"
#include "core/system/system_manager.hpp"
#include "core/system/system_modules.hpp"
#include "core/logging/logs/startup_logs.hpp"
#include "core/logging/logger/async_logger.hpp"


// =============================================================================
// MAIN APPLICATION ENTRY POINT
// =============================================================================

int main() {
    try {
        // Load system configuration
        AlpacaTrader::Config::SystemConfig initial_config;
        if (int result = load_system_config(initial_config)) {
            AlpacaTrader::Logging::log_message(std::string("ERROR: Config load failed with result: ") + std::to_string(result), "");
            return result;
        }
        
        // Create system state
        SystemState system_state(initial_config);
        
        // Initialize logging context
        system_state.logging_context = std::make_shared<AlpacaTrader::Logging::LoggingContext>();
        AlpacaTrader::Logging::set_logging_context(*system_state.logging_context);
        
        // Initialize application foundation (logging, validation)
        auto logger = AlpacaTrader::Logging::initialize_application_foundation(system_state.config);

        // Initialize CSV logging for bars and trades
        auto csv_bars_logger = AlpacaTrader::Logging::initialize_csv_bars_logger("bars_logs");
        auto csv_trade_logger = AlpacaTrader::Logging::initialize_csv_trade_logger("trade_logs");

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
