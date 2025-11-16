// main.cpp
#include "system/system_manager.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <memory>

using namespace AlpacaTrader::System;

// =============================================================================
// ENCAPSULATED SHUTDOWN HANDLER - NO GLOBAL VARIABLES
// =============================================================================
class ShutdownHandler {
private:
    std::atomic<bool> shutdown_requested_flag{false};
    SystemState* system_state_pointer = nullptr;

public:
    static ShutdownHandler& get_instance() {
        static ShutdownHandler instance;
        return instance;
    }

    void set_system_state(SystemState* state) {
        system_state_pointer = state;
    }

    bool is_shutdown_requested() const {
        return shutdown_requested_flag.load();
    }

    void signal_handler(int signal_number) {
        try {
            if (signal_number == SIGINT || signal_number == SIGTERM) {
                shutdown_requested_flag.store(true);
                if (system_state_pointer) {
                    system_state_pointer->running.store(false);
                    system_state_pointer->shutdown_requested.store(true);
                    system_state_pointer->cv.notify_all();
                }
            }
        } catch (...) {
            // Signal handler must not throw - force exit
            std::_Exit(1);
        }
    }

private:
    ShutdownHandler() = default;
    ShutdownHandler(const ShutdownHandler&) = delete;
    ShutdownHandler& operator=(const ShutdownHandler&) = delete;
};

// =============================================================================
// STATIC SIGNAL HANDLER FUNCTION
// =============================================================================
static void signal_handler(int signal_number) {
    ShutdownHandler::get_instance().signal_handler(signal_number);
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

        // Set system state for signal handler access (no global variables)
        ShutdownHandler::get_instance().set_system_state(initialization_result.system_state.get());

        // Start the complete trading system
        SystemThreads thread_handles = startup(*initialization_result.system_state, initialization_result.logger);

        // Run until shutdown signal
        run(*initialization_result.system_state);

        // Clean shutdown
        shutdown(*initialization_result.system_state, initialization_result.logger);

        return 0;
    } catch (const std::exception& exception_error) {
        // Ensure cleanup on exception
        try {
            // Get current system state if available
            auto& handler = ShutdownHandler::get_instance();
            if (handler.is_shutdown_requested()) {
                // Already shutting down, avoid double cleanup
                std::cerr << "Fatal error during shutdown: " << exception_error.what() << std::endl;
            } else {
                // Try to get system state for cleanup
                // Note: This is a limitation of removing globals - we can't access the system state here
                // The system should handle its own cleanup through proper exception handling
                std::cerr << "Fatal error: " << exception_error.what() << std::endl;
            }
        } catch (...) {
            // Ignore cleanup errors during exception handling
        }
        std::cerr << "Fatal error: " << exception_error.what() << std::endl;
        return 1;
    } catch (...) {
        // Ensure cleanup on unknown exception
        try {
            auto& handler = ShutdownHandler::get_instance();
            if (!handler.is_shutdown_requested()) {
                // System should handle its own cleanup through proper exception handling
                std::cerr << "Unknown fatal error occurred in main." << std::endl;
            }
        } catch (...) {
            // Ignore cleanup errors during exception handling
        }
        std::cerr << "Unknown fatal error occurred in main." << std::endl;
        return 1;
    }
}
