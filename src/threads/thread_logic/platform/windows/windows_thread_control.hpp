#ifndef WINDOWS_THREAD_CONTROL_HPP
#define WINDOWS_THREAD_CONTROL_HPP

#ifdef _WIN32

#include "configs/thread_config.hpp"
#include <thread>

namespace ThreadSystem {
namespace Platform {
namespace Windows {

class ThreadControl {
public:
    static bool set_priority(std::thread::native_handle_type handle, AlpacaTrader::Config::Priority priority, int cpu_affinity);
    static bool set_current_priority(AlpacaTrader::Config::Priority priority, int cpu_affinity);
    
private:
    static int priority_to_native(AlpacaTrader::Config::Priority priority);
};

} // namespace Windows
} // namespace Platform
} // namespace ThreadSystem

#endif // _WIN32
#endif // WINDOWS_THREAD_CONTROL_HPP
