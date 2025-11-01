// main.cpp
#include "system/system_manager.hpp"
#include <iostream>

using namespace AlpacaTrader::System;

// =============================================================================
// MAIN APPLICATION ENTRY POINT
// =============================================================================

int main() {
    try {
        // Initialize system - handles all setup (config loading, logging, etc.)
        SystemInitializationResult initialization_result = initialize();

        // Start the complete trading system
        SystemThreads thread_handles = startup(*initialization_result.system_state, initialization_result.logger);
        
        // Run until shutdown signal
        run(*initialization_result.system_state);
        
        // Clean shutdown
        shutdown(*initialization_result.system_state, initialization_result.logger);
        
        return 0;
    } catch (const std::exception& exception_error) {
        std::cerr << "Fatal error: " << exception_error.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred in main." << std::endl;
        return 1;
    }
}
