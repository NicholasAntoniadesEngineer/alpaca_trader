#ifndef LINUX_THREAD_CONTROL_HPP
#define LINUX_THREAD_CONTROL_HPP

#ifdef __linux__

#include "../../config/thread_config.hpp"
#include <thread>

namespace ThreadSystem {
namespace Platform {
namespace Linux {

class ThreadControl {
public:
    static bool set_priority(std::thread::native_handle_type handle, Priority priority, int cpu_affinity);
    static bool set_current_priority(Priority priority, int cpu_affinity);
    static std::string get_thread_info();
    static void set_thread_name(const std::string& name);
    
private:
    static int priority_to_native(Priority priority);
};

} // namespace Linux
} // namespace Platform
} // namespace ThreadSystem

#endif // __linux__
#endif // LINUX_THREAD_CONTROL_HPP
