#ifndef THREAD_CONTROL_HPP
#define THREAD_CONTROL_HPP

#include "configs/thread_config.hpp"
#include <thread>

namespace ThreadSystem {
namespace Platform {

// Cross-platform thread control interface
class ThreadControl {
public:
    // Set priority for a specific thread handle
    static bool set_priority(std::thread& thread, const AlpacaTrader::Config::ThreadSettings& config);
    
    // Set priority for the current thread
    static bool set_current_priority(const AlpacaTrader::Config::ThreadSettings& config);
    
    // Set priority with automatic fallback to lower priorities
    static AlpacaTrader::Config::Priority set_priority_with_fallback(std::thread& thread, const AlpacaTrader::Config::ThreadSettings& config);
    static AlpacaTrader::Config::Priority set_current_priority_with_fallback(const AlpacaTrader::Config::ThreadSettings& config);
    
    // Thread information utilities
    static std::string get_thread_info();
    static void set_thread_name(const std::string& name);
    
private:
    // Platform-specific implementations
    static bool set_thread_priority_impl(std::thread::native_handle_type handle, AlpacaTrader::Config::Priority priority, int cpu_affinity);
    static bool set_current_thread_priority_impl(AlpacaTrader::Config::Priority priority, int cpu_affinity);
};

} // namespace Platform
} // namespace ThreadSystem

#endif // THREAD_CONTROL_HPP
