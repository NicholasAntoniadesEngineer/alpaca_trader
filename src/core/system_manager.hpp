// system_manager.hpp
#ifndef SYSTEM_MANAGER_HPP
#define SYSTEM_MANAGER_HPP

// Forward declarations
struct SystemState;
struct SystemThreads;
class AsyncLogger;

namespace SystemManager {

// =============================================================================
// SYSTEM LIFECYCLE MANAGEMENT
// =============================================================================

/**
 * Startup the complete trading system
 * - Creates and configures all system components
 * - Sets up signal handling
 * - Starts all threads with proper configuration
 * @param system_state The system state containing configuration
 * @param logger The initialized logger
 * @return SystemThreads handles for the running system
 */
SystemThreads startup(SystemState& system_state, AsyncLogger& logger);

/**
 * Run the trading system until shutdown signal
 * - Monitors system operation
 * - Handles periodic monitoring and logging
 * @param system_state The system state
 * @param handles Thread handles for the running system
 */
void run(SystemState& system_state, SystemThreads& handles);

/**
 * Shutdown the trading system cleanly
 * - Signals all threads to stop
 * - Waits for clean thread termination
 * - Shuts down logging system
 * @param system_state The system state
 * @param handles Thread handles for the running system
 * @param logger The logger to shutdown
 */
void shutdown(SystemState& system_state, SystemThreads& handles, AsyncLogger& logger);

} // namespace SystemManager

#endif // SYSTEM_MANAGER_HPP
