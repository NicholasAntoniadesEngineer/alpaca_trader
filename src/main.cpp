// main.cpp
#include "system/system_manager.hpp"
#include <iostream>
#include <csignal>
#include <atomic>

using namespace AlpacaTrader::System;

// =============================================================================
// GLOBAL SHUTDOWN FLAG FOR SIGNAL HANDLER
// =============================================================================
static std::atomic<bool> shutdown_requested_flag{false};
static SystemState* global_system_state_pointer = nullptr;
static std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> global_logger_pointer = nullptr;

// =============================================================================
// SIGNAL HANDLER FOR GRACEFUL SHUTDOWN
// =============================================================================
static void signal_handler(int signal_number) {
    try {
        if (signal_number == SIGINT || signal_number == SIGTERM) {
            shutdown_requested_flag.store(true);
            if (global_system_state_pointer) {
                global_system_state_pointer->running.store(false);
                global_system_state_pointer->shutdown_requested.store(true);
                global_system_state_pointer->cv.notify_all();
            }
        }
    } catch (...) {
        // Signal handler must not throw - force exit
        std::_Exit(1);
    }
}

// =============================================================================
// MAIN APPLICATION ENTRY POINT
// =============================================================================

int main() {
    try {
        // Register signal handlers for graceful shutdown
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
        
        // Initialize system - handles all setup (config loading, logging, etc.)
        SystemInitializationResult initialization_result = initialize();
        
        // Store global pointers for signal handler access
        global_system_state_pointer = initialization_result.system_state.get();
        global_logger_pointer = initialization_result.logger;

        // Start the complete trading system
        SystemThreads thread_handles = startup(*initialization_result.system_state, initialization_result.logger);
        
        // Run until shutdown signal
        run(*initialization_result.system_state);
        
        // Clean shutdown
        shutdown(*initialization_result.system_state, initialization_result.logger);
        
        // Clear global pointers
        global_system_state_pointer = nullptr;
        global_logger_pointer.reset();
        
        return 0;
    } catch (const std::exception& exception_error) {
        // Ensure cleanup on exception
        if (global_system_state_pointer && global_logger_pointer) {
            try {
                shutdown(*global_system_state_pointer, global_logger_pointer);
            } catch (...) {
                // Ignore cleanup errors during exception handling
            }
        }
        std::cerr << "Fatal error: " << exception_error.what() << std::endl;
        return 1;
    } catch (...) {
        // Ensure cleanup on unknown exception
        if (global_system_state_pointer && global_logger_pointer) {
            try {
                shutdown(*global_system_state_pointer, global_logger_pointer);
            } catch (...) {
                // Ignore cleanup errors during exception handling
            }
        }
        std::cerr << "Unknown fatal error occurred in main." << std::endl;
        return 1;
    }
}
