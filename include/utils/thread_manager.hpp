#ifndef THREAD_MANAGER_HPP
#define THREAD_MANAGER_HPP

#include "thread_utils.hpp"
#include "../configs/timing_config.hpp"
#include <thread>
#include <atomic>
#include <chrono>

// Forward declarations to avoid circular dependencies
struct SystemThreads;

class ThreadManager {
public:
    // Setup thread priorities for the trading system
    static void setup_thread_priorities(SystemThreads& handles, const TimingConfig& config);
    
    // Log thread configuration information at startup
    static void log_thread_startup_info(const TimingConfig& config);
    
    // Log thread performance statistics
    static void log_thread_monitoring_stats(const SystemThreads& handles);
};

#endif // THREAD_MANAGER_HPP
