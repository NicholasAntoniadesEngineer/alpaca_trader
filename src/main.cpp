// main.cpp
#include "main.hpp"
#include "configs/config_loader.hpp"
#include "logging/startup_logger.hpp"
#include "logging/async_logger.hpp"
#include "core/system_manager.hpp"
#include <memory>

// =============================================================================
// MAIN APPLICATION ENTRY POINT
// =============================================================================

int main() {
    // Load system configuration
    SystemConfig initial_config;
    if (int result = load_system_config(initial_config)) return result;
    
    // Create system state
    SystemState system_state(initial_config);
    
    // Initialize application foundation (logging, validation)
    std::string timestamped_log_file = generate_timestamped_log_filename(system_state.config.logging.log_file);
    auto logger = std::make_shared<AsyncLogger>(timestamped_log_file);
    StartupLogger::initialize_application_foundation(system_state.config, *logger);
    
    // Start the complete trading system
    SystemThreads thread_handles = SystemManager::startup(system_state, logger);
    
    // Run until shutdown signal
    SystemManager::run(system_state, thread_handles);
    
    // Clean shutdown
    SystemManager::shutdown(system_state, thread_handles, logger);
    
    return 0;
}