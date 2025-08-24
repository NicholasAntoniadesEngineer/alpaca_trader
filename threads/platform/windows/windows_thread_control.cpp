#ifdef _WIN32

#include "threads/platform/windows/windows_thread_control.hpp"
#include <windows.h>
#include <processthreadsapi.h>
#include <sstream>

namespace ThreadSystem {
namespace Platform {
namespace Windows {

int ThreadControl::priority_to_native(Priority priority) {
    switch (priority) {
        case Priority::REALTIME: return THREAD_PRIORITY_TIME_CRITICAL;
        case Priority::HIGHEST:  return THREAD_PRIORITY_HIGHEST;
        case Priority::HIGH:     return THREAD_PRIORITY_ABOVE_NORMAL;
        case Priority::NORMAL:   return THREAD_PRIORITY_NORMAL;
        case Priority::LOW:      return THREAD_PRIORITY_BELOW_NORMAL;
        case Priority::LOWEST:   return THREAD_PRIORITY_LOWEST;
        default:                 return THREAD_PRIORITY_NORMAL;
    }
}

bool ThreadControl::set_priority(std::thread::native_handle_type handle, Priority priority, int cpu_affinity) {
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

bool ThreadControl::set_current_priority(Priority priority, int cpu_affinity) {
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

std::string ThreadControl::get_thread_info() {
    std::ostringstream oss;
    DWORD tid = GetCurrentThreadId();
    oss << "TID:" << tid;
    return oss.str();
}

void ThreadControl::set_thread_name(const std::string& name) {
    // Windows thread naming requires more complex setup
    // For now, we'll skip it to keep the implementation simple
    (void)name; // Suppress unused parameter warning
}

} // namespace Windows
} // namespace Platform
} // namespace ThreadSystem

#endif // _WIN32
