#include "utils/thread_utils.hpp"
#include "utils/async_logger.hpp"
#include <sstream>
#include <iomanip>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif __APPLE__
#include <pthread.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <mach/mach.h>
#elif _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#endif

bool ThreadUtils::set_thread_priority(std::thread& thread, const ThreadConfig& config) {
    bool success = true;
    
#ifdef __linux__
    pthread_t native_handle = thread.native_handle();
    
    // Set scheduling policy and priority
    struct sched_param param;
    param.sched_priority = priority_to_native(config.priority);
    
    int policy = (config.priority >= ThreadPriority::HIGHEST) ? SCHED_FIFO : SCHED_OTHER;
    
    if (pthread_setschedparam(native_handle, policy, &param) != 0) {
        success = false;
    }
    
#elif __APPLE__
    pthread_t native_handle = thread.native_handle();
    
    // macOS uses different approach with thread policies
    if (config.priority >= ThreadPriority::HIGH) {
        // Set time constraint policy for high priority threads
        thread_time_constraint_policy_data_t time_constraints;
        time_constraints.period = 1000000;      // 1ms in nanoseconds
        time_constraints.computation = 500000;   // 0.5ms computation time
        time_constraints.constraint = 1000000;   // 1ms constraint
        time_constraints.preemptible = TRUE;
        
        mach_port_t thread_port = pthread_mach_thread_np(native_handle);
        kern_return_t result = thread_policy_set(
            thread_port,
            THREAD_TIME_CONSTRAINT_POLICY,
            (thread_policy_t)&time_constraints,
            THREAD_TIME_CONSTRAINT_POLICY_COUNT
        );
        
        if (result != KERN_SUCCESS) {
            success = false;
        }
    } else {
        // Use standard priority for normal/low priority threads
        struct sched_param param;
        param.sched_priority = priority_to_native(config.priority);
        
        if (pthread_setschedparam(native_handle, SCHED_OTHER, &param) != 0) {
            success = false;
        }
    }
    
#elif _WIN32
    HANDLE native_handle = static_cast<HANDLE>(thread.native_handle());
    
    int win_priority;
    switch (config.priority) {
        case ThreadPriority::LOWEST:  win_priority = THREAD_PRIORITY_LOWEST; break;
        case ThreadPriority::LOW:     win_priority = THREAD_PRIORITY_BELOW_NORMAL; break;
        case ThreadPriority::NORMAL:  win_priority = THREAD_PRIORITY_NORMAL; break;
        case ThreadPriority::HIGH:    win_priority = THREAD_PRIORITY_ABOVE_NORMAL; break;
        case ThreadPriority::HIGHEST: win_priority = THREAD_PRIORITY_HIGHEST; break;
        case ThreadPriority::REALTIME: win_priority = THREAD_PRIORITY_TIME_CRITICAL; break;
        default: win_priority = THREAD_PRIORITY_NORMAL; break;
    }
    
    if (!SetThreadPriority(native_handle, win_priority)) {
        success = false;
    }
#endif

    // Set CPU affinity if specified
    if (config.cpu_affinity >= 0) {
        if (!set_cpu_affinity(thread, config.cpu_affinity)) {
            success = false;
        }
    }
    
    return success;
}

bool ThreadUtils::set_current_thread_priority(const ThreadConfig& config) {
    bool success = true;
    
#ifdef __linux__
    struct sched_param param;
    param.sched_priority = priority_to_native(config.priority);
    
    int policy = (config.priority >= ThreadPriority::HIGHEST) ? SCHED_FIFO : SCHED_OTHER;
    
    if (pthread_setschedparam(pthread_self(), policy, &param) != 0) {
        success = false;
    }
    
#elif __APPLE__
    if (config.priority >= ThreadPriority::HIGH) {
        thread_time_constraint_policy_data_t time_constraints;
        time_constraints.period = 1000000;
        time_constraints.computation = 500000;
        time_constraints.constraint = 1000000;
        time_constraints.preemptible = TRUE;
        
        kern_return_t result = thread_policy_set(
            mach_thread_self(),
            THREAD_TIME_CONSTRAINT_POLICY,
            (thread_policy_t)&time_constraints,
            THREAD_TIME_CONSTRAINT_POLICY_COUNT
        );
        
        if (result != KERN_SUCCESS) {
            success = false;
        }
    } else {
        struct sched_param param;
        param.sched_priority = priority_to_native(config.priority);
        
        if (pthread_setschedparam(pthread_self(), SCHED_OTHER, &param) != 0) {
            success = false;
        }
    }
    
#elif _WIN32
    int win_priority;
    switch (config.priority) {
        case ThreadPriority::LOWEST:  win_priority = THREAD_PRIORITY_LOWEST; break;
        case ThreadPriority::LOW:     win_priority = THREAD_PRIORITY_BELOW_NORMAL; break;
        case ThreadPriority::NORMAL:  win_priority = THREAD_PRIORITY_NORMAL; break;
        case ThreadPriority::HIGH:    win_priority = THREAD_PRIORITY_ABOVE_NORMAL; break;
        case ThreadPriority::HIGHEST: win_priority = THREAD_PRIORITY_HIGHEST; break;
        case ThreadPriority::REALTIME: win_priority = THREAD_PRIORITY_TIME_CRITICAL; break;
        default: win_priority = THREAD_PRIORITY_NORMAL; break;
    }
    
    if (!SetThreadPriority(GetCurrentThread(), win_priority)) {
        success = false;
    }
#endif

    // Set CPU affinity if specified
    if (config.cpu_affinity >= 0) {
        if (!set_current_cpu_affinity(config.cpu_affinity)) {
            success = false;
        }
    }
    
    // Set thread name for debugging
    if (!config.name.empty()) {
        set_thread_name(config.name);
    }
    
    return success;
}

ThreadPriority ThreadUtils::set_thread_priority_with_fallback(std::thread& thread, const ThreadConfig& config) {
    // Try the requested priority first
    if (set_thread_priority(thread, config)) {
        return config.priority;
    }
    
    // If that fails, try progressively lower priorities
    ThreadPriority current_priority = config.priority;
    ThreadConfig fallback_config = config;
    
    while (current_priority > ThreadPriority::LOWEST) {
        // Step down one priority level
        switch (current_priority) {
            case ThreadPriority::REALTIME:
                current_priority = ThreadPriority::HIGHEST;
                break;
            case ThreadPriority::HIGHEST:
                current_priority = ThreadPriority::HIGH;
                break;
            case ThreadPriority::HIGH:
                current_priority = ThreadPriority::NORMAL;
                break;
            case ThreadPriority::NORMAL:
                current_priority = ThreadPriority::LOW;
                break;
            case ThreadPriority::LOW:
                current_priority = ThreadPriority::LOWEST;
                break;
            default:
                return ThreadPriority::NORMAL; // Fallback to normal if something goes wrong
        }
        
        fallback_config.priority = current_priority;
        
        // Try setting this lower priority (first with affinity, then without)
        if (set_thread_priority(thread, fallback_config)) {
            return current_priority;
        }
        
        // If that fails, try without CPU affinity
        if (fallback_config.cpu_affinity >= 0) {
            fallback_config.cpu_affinity = -1; // Disable affinity
            if (set_thread_priority(thread, fallback_config)) {
                return current_priority;
            }
            // Reset affinity for next iteration
            fallback_config.cpu_affinity = config.cpu_affinity;
        }
    }
    
    // If even LOWEST fails, return NORMAL as indication of partial success
    return ThreadPriority::NORMAL;
}

ThreadPriority ThreadUtils::set_current_thread_priority_with_fallback(const ThreadConfig& config) {
    // Try the requested priority first
    if (set_current_thread_priority(config)) {
        return config.priority;
    }
    
    // If that fails, try progressively lower priorities
    ThreadPriority current_priority = config.priority;
    ThreadConfig fallback_config = config;
    
    while (current_priority > ThreadPriority::LOWEST) {
        // Step down one priority level
        switch (current_priority) {
            case ThreadPriority::REALTIME:
                current_priority = ThreadPriority::HIGHEST;
                break;
            case ThreadPriority::HIGHEST:
                current_priority = ThreadPriority::HIGH;
                break;
            case ThreadPriority::HIGH:
                current_priority = ThreadPriority::NORMAL;
                break;
            case ThreadPriority::NORMAL:
                current_priority = ThreadPriority::LOW;
                break;
            case ThreadPriority::LOW:
                current_priority = ThreadPriority::LOWEST;
                break;
            default:
                return ThreadPriority::NORMAL; // Fallback to normal if something goes wrong
        }
        
        fallback_config.priority = current_priority;
        
        // Try setting this lower priority (first with affinity, then without)
        if (set_current_thread_priority(fallback_config)) {
            return current_priority;
        }
        
        // If that fails, try without CPU affinity
        if (fallback_config.cpu_affinity >= 0) {
            fallback_config.cpu_affinity = -1; // Disable affinity
            if (set_current_thread_priority(fallback_config)) {
                return current_priority;
            }
            // Reset affinity for next iteration
            fallback_config.cpu_affinity = config.cpu_affinity;
        }
    }
    
    // If even LOWEST fails, return NORMAL as indication of partial success
    return ThreadPriority::NORMAL;
}

ThreadConfig ThreadUtils::get_default_config(ThreadType type) {
    switch (type) {
        case ThreadType::MAIN:
            return ThreadConfig(ThreadPriority::NORMAL, -1, "MAIN");
            
        case ThreadType::TRADER_DECISION:
            return ThreadConfig(ThreadPriority::HIGHEST, -1, "TRADER");  // No CPU pinning by default
            
        case ThreadType::MARKET_DATA:
            return ThreadConfig(ThreadPriority::HIGH, -1, "MARKET");     // No CPU pinning by default
            
        case ThreadType::ACCOUNT_DATA:
            return ThreadConfig(ThreadPriority::NORMAL, -1, "ACCOUNT");
            
        case ThreadType::MARKET_GATE:
            return ThreadConfig(ThreadPriority::LOW, -1, "GATE");
            
        case ThreadType::LOGGING:
            return ThreadConfig(ThreadPriority::LOWEST, -1, "LOGGER");
            
        default:
            return ThreadConfig(ThreadPriority::NORMAL, -1, "UNKNOWN");
    }
}

std::string ThreadUtils::get_thread_info() {
    std::ostringstream oss;
    
#ifdef __linux__
    pid_t tid = syscall(SYS_gettid);
    oss << "TID:" << tid;
    
    // Get current priority
    int policy;
    struct sched_param param;
    if (pthread_getschedparam(pthread_self(), &policy, &param) == 0) {
        oss << " Priority:" << param.sched_priority;
        oss << " Policy:" << (policy == SCHED_FIFO ? "FIFO" : 
                             policy == SCHED_RR ? "RR" : "OTHER");
    }
    
#elif __APPLE__
    uint64_t tid;
    pthread_threadid_np(nullptr, &tid);
    oss << "TID:" << tid;
    
#elif _WIN32
    oss << "TID:" << GetCurrentThreadId();
    oss << " Priority:" << GetThreadPriority(GetCurrentThread());
#endif
    
    return oss.str();
}

void ThreadUtils::set_thread_name(const std::string& name) {
#ifdef __linux__
    pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());  // Linux limits to 15 chars
#elif __APPLE__
    pthread_setname_np(name.c_str());
#elif _WIN32
    // Windows thread naming requires more complex setup
    // We'll skip this for now but could implement using SetThreadDescription
#endif
}

std::string ThreadUtils::priority_to_string(ThreadPriority priority) {
    switch (priority) {
        case ThreadPriority::LOWEST:  return "LOWEST";
        case ThreadPriority::LOW:     return "LOW";
        case ThreadPriority::NORMAL:  return "NORMAL";
        case ThreadPriority::HIGH:    return "HIGH";
        case ThreadPriority::HIGHEST: return "HIGHEST";
        case ThreadPriority::REALTIME: return "REALTIME";
        default: return "UNKNOWN";
    }
}

int ThreadUtils::priority_to_native(ThreadPriority priority) {
#ifdef __linux__
    switch (priority) {
        case ThreadPriority::LOWEST:  return -20;
        case ThreadPriority::LOW:     return -10;
        case ThreadPriority::NORMAL:  return 0;
        case ThreadPriority::HIGH:    return 10;
        case ThreadPriority::HIGHEST: return 20;
        case ThreadPriority::REALTIME: return 99;  // SCHED_FIFO priority
        default: return 0;
    }
#elif __APPLE__
    switch (priority) {
        case ThreadPriority::LOWEST:  return 1;
        case ThreadPriority::LOW:     return 10;
        case ThreadPriority::NORMAL:  return 31;  // Default priority
        case ThreadPriority::HIGH:    return 45;
        case ThreadPriority::HIGHEST: return 47;
        case ThreadPriority::REALTIME: return 63;
        default: return 31;
    }
#else
    return static_cast<int>(priority);
#endif
}

bool ThreadUtils::set_cpu_affinity(std::thread& thread, int cpu) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    
    pthread_t native_handle = thread.native_handle();
    return pthread_setaffinity_np(native_handle, sizeof(cpu_set_t), &cpuset) == 0;
    
#elif __APPLE__
    // macOS doesn't support hard CPU affinity, but we can set thread affinity tags
    thread_affinity_policy_data_t policy;
    policy.affinity_tag = cpu;
    
    pthread_t native_handle = thread.native_handle();
    mach_port_t thread_port = pthread_mach_thread_np(native_handle);
    
    kern_return_t result = thread_policy_set(
        thread_port,
        THREAD_AFFINITY_POLICY,
        (thread_policy_t)&policy,
        THREAD_AFFINITY_POLICY_COUNT
    );
    
    return result == KERN_SUCCESS;
    
#elif _WIN32
    HANDLE native_handle = static_cast<HANDLE>(thread.native_handle());
    DWORD_PTR mask = 1ULL << cpu;
    return SetThreadAffinityMask(native_handle, mask) != 0;
    
#else
    return false;  // Not supported on this platform
#endif
}

bool ThreadUtils::set_current_cpu_affinity(int cpu) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
    
#elif __APPLE__
    thread_affinity_policy_data_t policy;
    policy.affinity_tag = cpu;
    
    kern_return_t result = thread_policy_set(
        mach_thread_self(),
        THREAD_AFFINITY_POLICY,
        (thread_policy_t)&policy,
        THREAD_AFFINITY_POLICY_COUNT
    );
    
    return result == KERN_SUCCESS;
    
#elif _WIN32
    DWORD_PTR mask = 1ULL << cpu;
    return SetThreadAffinityMask(GetCurrentThread(), mask) != 0;
    
#else
    return false;
#endif
}

// ScopedThreadPriority implementation
ScopedThreadPriority::ScopedThreadPriority(const ThreadConfig& config) : changed(false) {
    // Store original config (simplified - just store if we changed anything)
    changed = ThreadUtils::set_current_thread_priority(config);
}

ScopedThreadPriority::~ScopedThreadPriority() {
    if (changed) {
        // Restore to normal priority
        ThreadConfig normal_config(ThreadPriority::NORMAL);
        ThreadUtils::set_current_thread_priority(normal_config);
    }
}
