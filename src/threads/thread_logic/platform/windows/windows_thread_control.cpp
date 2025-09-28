#ifdef _WIN32

#include "windows_thread_control.hpp"
#include <windows.h>
#include <processthreadsapi.h>

namespace ThreadSystem {
namespace Platform {
namespace Windows {

int ThreadControl::priority_to_native(AlpacaTrader::Config::Priority priority) {
    switch (priority) {
        case AlpacaTrader::Config::Priority::REALTIME: return THREAD_PRIORITY_TIME_CRITICAL;
        case AlpacaTrader::Config::Priority::HIGHEST:  return THREAD_PRIORITY_HIGHEST;
        case AlpacaTrader::Config::Priority::HIGH:     return THREAD_PRIORITY_ABOVE_NORMAL;
        case AlpacaTrader::Config::Priority::NORMAL:   return THREAD_PRIORITY_NORMAL;
        case AlpacaTrader::Config::Priority::LOW:      return THREAD_PRIORITY_BELOW_NORMAL;
        case AlpacaTrader::Config::Priority::LOWEST:   return THREAD_PRIORITY_LOWEST;
        default:                 return THREAD_PRIORITY_NORMAL;
    }
}

bool ThreadControl::set_priority(std::thread::native_handle_type handle, AlpacaTrader::Config::Priority priority, int cpu_affinity) {
    HANDLE thread_handle = static_cast<HANDLE>(handle);
    bool success = true;
    
    // Set thread priority
    if (!SetThreadPriority(thread_handle, priority_to_native(priority))) {
        success = false;
    }
    
    // Set CPU affinity if specified
    if (success && cpu_affinity >= 0) {
        DWORD_PTR affinity_mask = 1ULL << cpu_affinity;
        if (SetThreadAffinityMask(thread_handle, affinity_mask) == 0) {
            success = false;
        }
    }
    
    return success;
}

bool ThreadControl::set_current_priority(AlpacaTrader::Config::Priority priority, int cpu_affinity) {
    HANDLE current_thread = GetCurrentThread();
    bool success = true;
    
    if (!SetThreadPriority(current_thread, priority_to_native(priority))) {
        success = false;
    }
    
    // Set CPU affinity if specified
    if (success && cpu_affinity >= 0) {
        DWORD_PTR affinity_mask = 1ULL << cpu_affinity;
        if (SetThreadAffinityMask(current_thread, affinity_mask) == 0) {
            success = false;
        }
    }
    
    return success;
}


} // namespace Windows
} // namespace Platform
} // namespace ThreadSystem

#endif // _WIN32
