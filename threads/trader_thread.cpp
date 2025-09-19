#include "trader_thread.hpp"
#include "../logging/async_logger.hpp"
#include "../logging/startup_logger.hpp"
#include "platform/thread_control.hpp"
#include <chrono>
#include <thread>

void TraderThread::operator()() {
    set_log_thread_tag("DECIDE");
    
    // Set iteration counter for monitoring
    trader.set_iteration_counter(trader_iterations);
    
    // Wait for main thread to complete priority setup
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Start the trader's decision loop
    trader.decision_loop();
}
