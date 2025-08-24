#ifdef __linux__

#include "threads/platform/linux/linux_thread_control.hpp"
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sstream>

namespace ThreadSystem {
namespace Platform {
namespace Linux {

int ThreadControl::priority_to_native(Priority priority) {
    switch (priority) {
        case Priority::REALTIME: return 80;
        case Priority::HIGHEST:  return 60;
        case Priority::HIGH:     return 40;
        case Priority::NORMAL:   return 20;
        case Priority::LOW:      return 10;
        case Priority::LOWEST:   return 1;
        default:                 return 20;
    }
}

bool ThreadControl::set_priority(std::thread::native_handle_type handle, Priority priority, int cpu_affinity) {
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

bool ThreadControl::set_current_priority(Priority priority, int cpu_affinity) {
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

std::string ThreadControl::get_thread_info() {
    std::ostringstream oss;
    pid_t tid = syscall(SYS_gettid);
    oss << "TID:" << tid;
    return oss.str();
}

void ThreadControl::set_thread_name(const std::string& name) {
    pthread_setname_np(pthread_self(), name.c_str());
}

} // namespace Linux
} // namespace Platform
} // namespace ThreadSystem

#endif // __linux__
