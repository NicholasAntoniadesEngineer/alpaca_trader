// main.cpp
#include "configs/config_loader.hpp"
#include "configs/system_config.hpp"
#include "core/system_state.hpp"
#include "core/system_threads.hpp"
#include "core/system_manager.hpp"
#include "core/trading_system_modules.hpp"
#include "logging/startup_logger.hpp"


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
    auto logger = StartupLogger::initialize_application_foundation(system_state.config);
    
    // Start the complete trading system
    SystemThreads thread_handles = SystemManager::startup(system_state, logger);
    
    // Run until shutdown signal
    SystemManager::run(system_state, thread_handles);
    
    // Clean shutdown
    SystemManager::shutdown(system_state, thread_handles, logger);
    
    return 0;
}
