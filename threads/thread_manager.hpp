#ifndef THREAD_MANAGER_HPP
#define THREAD_MANAGER_HPP

#include "config/thread_config.hpp"
#include "platform/thread_control.hpp"
#include "../configs/timing_config.hpp"

// Forward declarations
struct SystemThreads;

namespace ThreadSystem {

// High-level thread management interface
class Manager {
public:
    // Setup thread priorities for all system threads
    static void setup_thread_priorities(SystemThreads& handles, const TimingConfig& config);
    
    // Logging utilities
    static void log_thread_startup_info(const TimingConfig& config);
    static void log_thread_monitoring_stats(const SystemThreads& handles);
    
private:
    // Internal thread setup structure
    struct ThreadSetup {
        std::string name;
        Type type;
        std::thread& handle;
        std::string priority_tag;
        bool uses_cpu_affinity;
        int cpu_core;
        
        ThreadSetup(const std::string& n, Type t, std::thread& h, 
                    const std::string& tag, bool affinity = false, int core = -1)
            : name(n), type(t), handle(h), priority_tag(tag), uses_cpu_affinity(affinity), cpu_core(core) {}
    };
    
    // Configure a single thread
    static void configure_single_thread(const ThreadSetup& setup, const TimingConfig& config);
};

} // namespace ThreadSystem

#endif // THREAD_MANAGER_HPP
