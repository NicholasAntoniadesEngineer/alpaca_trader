#ifndef THREAD_TYPES_HPP
#define THREAD_TYPES_HPP

#include <string>

namespace AlpacaTrader {
namespace Config {

// Thread priority levels
enum class Priority {
    REALTIME,
    HIGHEST,
    HIGH,
    NORMAL,
    LOW,
    LOWEST
};

// Thread settings structure
struct ThreadSettings {
    Priority priority;
    int cpu_affinity;  
    std::string name;
    bool use_cpu_affinity;   
    
    ThreadSettings(Priority p = Priority::NORMAL, int affinity = -1, const std::string& n = "", bool use_affinity = false)
        : priority(p), cpu_affinity(affinity), name(n), use_cpu_affinity(use_affinity) {}
};

// Thread status data for monitoring
struct ThreadStatusData {
    std::string name;
    std::string priority;
    bool success;
    int cpu_core;
    std::string status_message;
    
    ThreadStatusData(const std::string& thread_name, const std::string& priority_level, bool config_success, int cpu = -1, const std::string& message = "")
        : name(thread_name), priority(priority_level), success(config_success), cpu_core(cpu), status_message(message) {}
};

} // namespace Config
} // namespace AlpacaTrader

#endif // THREAD_TYPES_HPP
