#ifdef __APPLE__

#include "threads/platform/macos/macos_thread_control.hpp"
#include "configs/thread_config.hpp"
#include <pthread.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <mach/mach.h>
#include <sstream>

namespace ThreadSystem {
namespace Platform {
namespace MacOS {

int ThreadControl::priority_to_native(AlpacaTrader::Config::Priority priority) {
    switch (priority) {
        case AlpacaTrader::Config::Priority::REALTIME: return 47;
        case AlpacaTrader::Config::Priority::HIGHEST:  return 40;
        case AlpacaTrader::Config::Priority::HIGH:     return 35;
        case AlpacaTrader::Config::Priority::NORMAL:   return 31;  // Default macOS thread priority
        case AlpacaTrader::Config::Priority::LOW:      return 25;
        case AlpacaTrader::Config::Priority::LOWEST:   return 15;
        default:                 return 31;
    }
}

bool ThreadControl::set_priority(std::thread::native_handle_type handle, AlpacaTrader::Config::Priority priority, int cpu_affinity) {
    pthread_t native_handle = handle;
    bool success = true;
    
    // macOS uses different approach with thread policies
    if (priority >= AlpacaTrader::Config::Priority::HIGH) {
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
        // Set standard priority for normal/low priority threads
        thread_precedence_policy_data_t precedence;
        precedence.importance = priority_to_native(priority);
        
        mach_port_t thread_port = pthread_mach_thread_np(native_handle);
        kern_return_t result = thread_policy_set(
            thread_port,
            THREAD_PRECEDENCE_POLICY,
            (thread_policy_t)&precedence,
            THREAD_PRECEDENCE_POLICY_COUNT
        );
        
        if (result != KERN_SUCCESS) {
            success = false;
        }
    }
    
    // Note: macOS doesn't support CPU affinity in the same way as Linux
    // CPU affinity requests are ignored but don't cause failure
    (void)cpu_affinity;
    
    return success;
}

bool ThreadControl::set_current_priority(AlpacaTrader::Config::Priority priority, int cpu_affinity) {
    bool success = true;
    

    if (priority >= AlpacaTrader::Config::Priority::HIGH) {
        thread_time_constraint_policy_data_t time_constraints;
        time_constraints.period = 1000000;
        time_constraints.computation = 500000;
        time_constraints.constraint = 1000000;
        time_constraints.preemptible = TRUE;
        
        mach_port_t current_thread_port = mach_thread_self();
        kern_return_t result = thread_policy_set(
            current_thread_port,
            THREAD_TIME_CONSTRAINT_POLICY,
            (thread_policy_t)&time_constraints,
            THREAD_TIME_CONSTRAINT_POLICY_COUNT
        );
        
        if (result != KERN_SUCCESS) {
            success = false;
        }
        
        mach_port_deallocate(mach_task_self(), current_thread_port);
    } else {
        thread_precedence_policy_data_t precedence;
        precedence.importance = priority_to_native(priority);
        
        mach_port_t current_thread_port = mach_thread_self();
        kern_return_t result = thread_policy_set(
            current_thread_port,
            THREAD_PRECEDENCE_POLICY,
            (thread_policy_t)&precedence,
            THREAD_PRECEDENCE_POLICY_COUNT
        );
        
        if (result != KERN_SUCCESS) {
            success = false;
        }
        
        mach_port_deallocate(mach_task_self(), current_thread_port);
    }
    
    // CPU affinity not supported on macOS
    (void)cpu_affinity;
    
    return success;
}

std::string ThreadControl::get_thread_info() {
    std::ostringstream oss;
    uint64_t tid;
    pthread_threadid_np(NULL, &tid);
    oss << "TID:" << tid;
    return oss.str();
}

void ThreadControl::set_thread_name(const std::string& name) {
    pthread_setname_np(name.c_str());
}

} // namespace MacOS
} // namespace Platform
} // namespace ThreadSystem

#endif // __APPLE__