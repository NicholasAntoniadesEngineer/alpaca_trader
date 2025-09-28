#ifndef LINUX_THREAD_CONTROL_HPP
#define LINUX_THREAD_CONTROL_HPP

#ifdef __linux__

#include "configs/thread_config.hpp"
#include <thread>

namespace ThreadSystem {
namespace Platform {
namespace Linux {

class ThreadControl {
public:
    static bool set_priority(std::thread::native_handle_type handle, AlpacaTrader::Config::Priority priority, int cpu_affinity);
    static bool set_current_priority(AlpacaTrader::Config::Priority priority, int cpu_affinity);
    
private:
    static int priority_to_native(AlpacaTrader::Config::Priority priority);
};

} // namespace Linux
} // namespace Platform
} // namespace ThreadSystem

#endif // __linux__
#endif // LINUX_THREAD_CONTROL_HPP
