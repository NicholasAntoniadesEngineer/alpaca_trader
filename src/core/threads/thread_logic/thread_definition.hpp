#ifndef THREAD_DEFINITION_HPP
#define THREAD_DEFINITION_HPP

#include "core/threads/thread_register.hpp"
#include "core/system/system_modules.hpp"
#include "core/system/system_threads.hpp"
#include <functional>
#include <atomic>

namespace AlpacaTrader {
namespace Core {
namespace ThreadSystem {

struct ThreadDefinition {
    std::string identifier;
    std::string name;  // For compatibility with old code
    std::function<void(SystemModules&)> get_function;
    std::function<void()> thread_function;  // For compatibility with old code
    std::function<void(SystemModules&, std::atomic<unsigned long>&)> set_iteration_counter;
    std::function<std::atomic<unsigned long>&(SystemThreads&)> get_counter;
    std::function<AlpacaTrader::Config::ThreadSettings(const AlpacaTrader::Config::SystemConfig&)> get_config;
    std::atomic<unsigned long>& iteration_counter;  // For compatibility with old code
    bool uses_cpu_affinity = false;  // For compatibility with old code
    int cpu_core = -1;  // For compatibility with old code
    
    ThreadDefinition(const std::string& id,
                     std::function<void(SystemModules&)> func,
                     std::function<void(SystemModules&, std::atomic<unsigned long>&)> set_counter,
                     std::function<std::atomic<unsigned long>&(SystemThreads&)> get_counter_func,
                     std::function<AlpacaTrader::Config::ThreadSettings(const AlpacaTrader::Config::SystemConfig&)> config_func,
                     std::atomic<unsigned long>& counter)
        : identifier(id), name(id), get_function(func), thread_function(), set_iteration_counter(set_counter), get_counter(get_counter_func), get_config(config_func), iteration_counter(counter) {}
    
    // Method to set thread_function after construction
    void set_thread_function(std::function<void()> func) {
        thread_function = func;
    }
};

} // namespace ThreadSystem
} // namespace Core
} // namespace AlpacaTrader

#endif // THREAD_DEFINITION_HPP
