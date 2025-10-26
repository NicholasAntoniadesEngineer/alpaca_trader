/**
 * Trader decision thread.
 * Executes the main trading logic and decision making.
 */
#include "trader_thread.hpp"
#include "core/trader/trader.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/startup_logs.hpp"
#include "core/logging/thread_logs.hpp"
#include <chrono>
#include <thread>

// Using declarations for cleaner code
using namespace AlpacaTrader::Threads;
using namespace AlpacaTrader::Logging;

// ========================================================================
// THREAD LIFECYCLE MANAGEMENT
// ========================================================================

void TraderThread::operator()() {
    set_log_thread_tag("DECIDE");

    try {
        // Wait for main thread to complete priority setup and other threads to initialize
        std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_sequence_delay_milliseconds + 2000)); // Extra 2 seconds

        // Start the trader's decision loop
        trader.execute_trading_loop();
    } catch (const std::exception& exception) {
        ThreadLogs::log_thread_exception("TraderThread", std::string(exception.what()));
        log_message("TraderThread exception: " + std::string(exception.what()), "trading_system.log");
    } catch (...) {
        ThreadLogs::log_thread_unknown_exception("TraderThread");
        log_message("TraderThread unknown exception", "trading_system.log");
    }
}
