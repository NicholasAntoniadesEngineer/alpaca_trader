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
    // Strict behavior: no fallbacks allowed. Either exact config applies or we fail.
    if (set_priority(thread, config)) {
        return config.priority;
    }
    throw std::runtime_error("Thread priority/affinity configuration failed on this platform");
}


} // namespace Platform
} // namespace ThreadSystem