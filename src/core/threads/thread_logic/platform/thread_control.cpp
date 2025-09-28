#include "thread_control.hpp"

#ifdef __linux__
#include "linux/linux_thread_control.hpp"
#elif __APPLE__
#include "macos/macos_thread_control.hpp"
#elif _WIN32
#include "windows/windows_thread_control.hpp"
#endif

namespace ThreadSystem {
namespace Platform {

bool ThreadControl::set_priority(std::thread& thread, const AlpacaTrader::Config::ThreadSettings& config) {
#ifdef __linux__
    return Linux::ThreadControl::set_priority(thread.native_handle(), config.priority, config.cpu_affinity);
#elif __APPLE__
    return MacOS::ThreadControl::set_priority(thread.native_handle(), config.priority, config.cpu_affinity);
#elif _WIN32
    return Windows::ThreadControl::set_priority(thread.native_handle(), config.priority, config.cpu_affinity);
#else
    return false; // Platform not supported
#endif
}

bool ThreadControl::set_current_priority(const AlpacaTrader::Config::ThreadSettings& config) {
#ifdef __linux__
    return Linux::ThreadControl::set_current_priority(config.priority, config.cpu_affinity);
#elif __APPLE__
    return MacOS::ThreadControl::set_current_priority(config.priority, config.cpu_affinity);
#elif _WIN32
    return Windows::ThreadControl::set_current_priority(config.priority, config.cpu_affinity);
#else
    return false; // Platform not supported
#endif
}

AlpacaTrader::Config::Priority ThreadControl::set_priority_with_fallback(std::thread& thread, const AlpacaTrader::Config::ThreadSettings& config) {
    // Try original priority first
    if (set_priority(thread, config)) {
        return config.priority;
    }
    
    // Create fallback configuration
    AlpacaTrader::Config::ThreadSettings fallback_config = config;
    
    // Try progressively lower priorities
    AlpacaTrader::Config::Priority priorities[] = {AlpacaTrader::Config::Priority::HIGH, AlpacaTrader::Config::Priority::NORMAL, AlpacaTrader::Config::Priority::LOW, AlpacaTrader::Config::Priority::LOWEST};
    
    for (AlpacaTrader::Config::Priority current_priority : priorities) {
        if (current_priority >= config.priority) continue;  // Skip if not actually lower
        
        fallback_config.priority = current_priority;
        
        // Try with CPU affinity first
        if (set_priority(thread, fallback_config)) {
            return current_priority;
        }
        
        // If that fails, try without CPU affinity
        if (fallback_config.cpu_affinity >= 0) {
            fallback_config.cpu_affinity = -1; // Disable affinity
            if (set_priority(thread, fallback_config)) {
                return current_priority;
            }
            // Reset affinity for next iteration
            fallback_config.cpu_affinity = config.cpu_affinity;
        }
    }
    
    // If even LOWEST fails, return NORMAL as indication of partial success
    return AlpacaTrader::Config::Priority::NORMAL;
}


} // namespace Platform
} // namespace ThreadSystem