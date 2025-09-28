#ifndef THREAD_CONFIG_HPP
#define THREAD_CONFIG_HPP

#include <string>
#include <atomic>
#include <functional>

// Forward declarations
struct TradingSystemModules;
struct SystemThreads;

// Forward declaration
struct SystemConfig;

namespace AlpacaTrader {
namespace Config {

// Thread priority levels (cross-platform abstraction)
enum class Priority {
    REALTIME,
    HIGHEST,
    HIGH,
    NORMAL,
    LOW,
    LOWEST
};

struct ThreadSettings {
    Priority priority;
    int cpu_affinity;  
    std::string name;
    bool use_cpu_affinity;   
    
    ThreadSettings(Priority p = Priority::NORMAL, int affinity = -1, const std::string& n = "", bool use_affinity = false)
        : priority(p), cpu_affinity(affinity), name(n), use_cpu_affinity(use_affinity) {}
};


class ConfigProvider {
public:
    static std::string priority_to_string(Priority priority);
    static Priority string_to_priority(const std::string& str);
};

struct ThreadStatusData {
    std::string name;
    std::string priority;
    bool success;
    int cpu_core;
    std::string status_message;
    
    ThreadStatusData(const std::string& thread_name, const std::string& priority_level, bool config_success, int cpu = -1, const std::string& message = "")
        : name(thread_name), priority(priority_level), success(config_success), cpu_core(cpu), status_message(message) {}
};

struct ThreadMetadata {
    std::string identifier;  // Single source of truth (e.g., "MARKET", "ACCOUNT", etc.)
    std::function<void(TradingSystemModules&)> get_function;
    std::function<std::atomic<unsigned long>&(SystemThreads&)> get_counter;
    
    // All thread-specific information derived from identifier
    std::string get_display_name() const;
    std::string get_config_type() const;
    std::string get_log_tag() const;
    
    ThreadMetadata(const std::string& thread_id,
                   std::function<void(TradingSystemModules&)> func,
                   std::function<std::atomic<unsigned long>&(SystemThreads&)> counter)
        : identifier(thread_id), get_function(func), get_counter(counter) {}
};

} // namespace Config
} // namespace AlpacaTrader

#endif // THREAD_CONFIG_HPP
