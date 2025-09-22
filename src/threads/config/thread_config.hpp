#ifndef THREAD_CONFIG_HPP
#define THREAD_CONFIG_HPP

#include <string>

namespace ThreadSystem {

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
    
    ThreadConfig(Priority p = Priority::NORMAL, int affinity = -1, const std::string& n = "")
        : priority(p), cpu_affinity(affinity), name(n) {}
};

// Thread configuration provider
class ConfigProvider {
public:
    static ThreadConfig get_default_config(Type type);
    static std::string priority_to_string(Priority priority);
    static Priority string_to_priority(const std::string& str);
};

} // namespace ThreadSystem

#endif // THREAD_CONFIG_HPP
