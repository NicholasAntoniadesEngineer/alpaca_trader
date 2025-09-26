#include "threads/trader_thread.hpp"
#include "logging/async_logger.hpp"
#include "logging/startup_logger.hpp"
#include <chrono>
#include <thread>

// Using declarations for cleaner code
using AlpacaTrader::Threads::TraderThread;
using AlpacaTrader::Logging::set_log_thread_tag;

void TraderThread::operator()() {
    set_log_thread_tag("DECIDE");
    
    // Set iteration counter for monitoring
    trader.set_iteration_counter(trader_iterations);
    
    // Wait for main thread to complete priority setup
    std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_delay_ms));
    
    // Start the trader's decision loop
    trader.decision_loop();
}
