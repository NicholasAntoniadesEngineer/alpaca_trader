#ifndef THREAD_CONFIG_HPP
#define THREAD_CONFIG_HPP

#include <string>
#include <atomic>
#include <functional>
#include "threads/thread_register.hpp"
#include "system/system_modules.hpp"
#include "system/system_threads.hpp"
#include "system_config.hpp"

namespace AlpacaTrader {
namespace Config {


class ConfigProvider {
public:
    static std::string priority_to_string(Priority priority);
    static Priority string_to_priority(const std::string& str);
};


struct ThreadMetadata {
    std::string identifier;  // Single source of truth (e.g., "MARKET", "ACCOUNT", etc.)
    std::function<void(SystemModules&)> get_function;
    std::function<std::atomic<unsigned long>&(SystemThreads&)> get_counter;
    
    // All thread-specific information derived from identifier
    std::string get_display_name() const;
    std::string get_config_type() const;
    std::string get_log_tag() const;
    
    ThreadMetadata(const std::string& thread_id,
                   std::function<void(SystemModules&)> func,
                   std::function<std::atomic<unsigned long>&(SystemThreads&)> counter)
        : identifier(thread_id), get_function(func), get_counter(counter) {}
};

} // namespace Config
} // namespace AlpacaTrader

#endif // THREAD_CONFIG_HPP
