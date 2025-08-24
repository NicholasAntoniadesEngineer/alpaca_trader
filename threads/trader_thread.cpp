#include "trader_thread.hpp"
#include "../utils/async_logger.hpp"
#include "platform/thread_control.hpp"
#include <chrono>
#include <thread>

void TraderThread::operator()() {
    set_log_thread_tag("DECIDE");
    log_message("   |  • Trader decision thread started: " + ThreadSystem::Platform::ThreadControl::get_thread_info(), "");
    
    // Wait for main thread to complete priority setup
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Start the trader's decision loop
    trader.decision_loop();
}
