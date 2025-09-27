#ifndef THREAD_CONFIG_HPP
#define THREAD_CONFIG_HPP

#include <string>
#include <atomic>
#include <functional>

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

// Thread types in the system
enum class Type {
    MAIN,
    TRADER_DECISION,
    MARKET_DATA,
    ACCOUNT_DATA,
    MARKET_GATE,
    LOGGING
};

// Generic thread configuration
struct ThreadConfig {
    Priority priority;
    int cpu_affinity;  // -1 = no affinity, >=0 = specific core
    std::string name;
    bool use_cpu_affinity;  // Whether this thread type should use CPU affinity
    
    ThreadConfig(Priority p = Priority::NORMAL, int affinity = -1, const std::string& n = "", bool use_affinity = false)
        : priority(p), cpu_affinity(affinity), name(n), use_cpu_affinity(use_affinity) {}
};

// Thread configuration container for all thread types
struct ThreadConfigs {
    ThreadConfig main;
    ThreadConfig trader_decision;
    ThreadConfig market_data;
    ThreadConfig account_data;
    ThreadConfig market_gate;
    ThreadConfig logging;
    
    ThreadConfigs() = default;
};

// Thread configuration provider
class ConfigProvider {
public:
    static ThreadConfig get_config_from_system(Type type, const SystemConfig& system_config);
    static std::string priority_to_string(Priority priority);
    static Priority string_to_priority(const std::string& str);
};

// Thread manager configuration structure
struct ThreadManagerConfig {
    std::string name;
    std::function<void()> thread_function;
    std::atomic<unsigned long>& iteration_counter;
    Type config_type;
    
    ThreadManagerConfig(const std::string& n, std::function<void()> func, 
                       std::atomic<unsigned long>& counter, Type type)
        : name(n), thread_function(std::move(func)), iteration_counter(counter), config_type(type) {}
};

} // namespace Config
} // namespace AlpacaTrader

#endif // THREAD_CONFIG_HPP
