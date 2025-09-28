#include "trader_thread.hpp"
#include "logging/async_logger.hpp"
#include "logging/startup_logs.hpp"
#include "logging/thread_logs.hpp"
#include <chrono>
#include <thread>

// Using declarations for cleaner code
using AlpacaTrader::Threads::TraderThread;
using AlpacaTrader::Logging::set_log_thread_tag;
using AlpacaTrader::Logging::log_message;

void TraderThread::operator()() {
    set_log_thread_tag("DECIDE");

    try {
        // Set iteration counter for monitoring
        trader.set_iteration_counter(*trader_iterations);

        // Wait for main thread to complete priority setup and other threads to initialize
        std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_delay_ms + 2000)); // Extra 2 seconds

        // Start the trader's decision loop
        trader.decision_loop();
    } catch (const std::exception& e) {
        ThreadLogs::log_thread_exception("TraderThread", std::string(e.what()));
        log_message("TraderThread exception: " + std::string(e.what()), "trading_system.log");
    } catch (...) {
        ThreadLogs::log_thread_unknown_exception("TraderThread");
        log_message("TraderThread unknown exception", "trading_system.log");
    }
}
