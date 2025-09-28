#ifdef __linux__

#include "linux_thread_control.hpp"
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace ThreadSystem {
namespace Platform {
namespace Linux {

int ThreadControl::priority_to_native(AlpacaTrader::Config::Priority priority) {
    switch (priority) {
        case AlpacaTrader::Config::Priority::REALTIME: return 80;
        case AlpacaTrader::Config::Priority::HIGHEST:  return 60;
        case AlpacaTrader::Config::Priority::HIGH:     return 40;
        case AlpacaTrader::Config::Priority::NORMAL:   return 20;
        case AlpacaTrader::Config::Priority::LOW:      return 10;
        case AlpacaTrader::Config::Priority::LOWEST:   return 1;
        default:                 return 20;
    }
}

bool ThreadControl::set_priority(std::thread::native_handle_type handle, AlpacaTrader::Config::Priority priority, int cpu_affinity) {
    pthread_t native_handle = handle;
    bool success = true;
    
    // Set scheduling policy and priority
    struct sched_param param;
    param.sched_priority = priority_to_native(priority);
    
    int policy = (priority >= Priority::HIGHEST) ? SCHED_FIFO : SCHED_OTHER;
    
    if (pthread_setschedparam(native_handle, policy, &param) != 0) {
        success = false;
    }
    
    // Set CPU affinity if specified
    if (success && cpu_affinity >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_affinity, &cpuset);
        
        if (pthread_setaffinity_np(native_handle, sizeof(cpu_set_t), &cpuset) != 0) {
            success = false;
        }
    }
    
    return success;
}

bool ThreadControl::set_current_priority(AlpacaTrader::Config::Priority priority, int cpu_affinity) {
    pthread_t current_thread = pthread_self();
    bool success = true;
    
    // Set scheduling policy and priority
    struct sched_param param;
    param.sched_priority = priority_to_native(priority);
    
    int policy = (priority >= Priority::HIGHEST) ? SCHED_FIFO : SCHED_OTHER;
    
    if (pthread_setschedparam(current_thread, policy, &param) != 0) {
        success = false;
    }
    
    // Set CPU affinity if specified
    if (success && cpu_affinity >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_affinity, &cpuset);
        
        if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
            success = false;
        }
    }
    
    return success;
}


} // namespace Linux
} // namespace Platform
} // namespace ThreadSystem

#endif // __linux__
