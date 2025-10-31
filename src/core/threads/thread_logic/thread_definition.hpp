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
    std::string name;
    std::function<void(SystemModules&)> get_function;
    std::function<void()> thread_function;
    std::function<void(SystemModules&, std::atomic<unsigned long>&)> set_iteration_counter;
    std::function<std::atomic<unsigned long>&(SystemThreads&)> get_counter;
    std::function<AlpacaTrader::Config::ThreadSettings(const AlpacaTrader::Config::SystemConfig&)> get_config;
    std::atomic<unsigned long>& iteration_counter;
    bool uses_cpu_affinity;
    int cpu_core;
    
    ThreadDefinition(const std::string& id,
                     std::function<void(SystemModules&)> func,
                     std::function<void(SystemModules&, std::atomic<unsigned long>&)> set_counter,
                     std::function<std::atomic<unsigned long>&(SystemThreads&)> get_counter_func,
                     std::function<AlpacaTrader::Config::ThreadSettings(const AlpacaTrader::Config::SystemConfig&)> config_func,
                     std::atomic<unsigned long>& counter,
                     bool cpu_affinity_enabled,
                     int assigned_cpu_core)
        : identifier(id), name(id), get_function(func), thread_function(), set_iteration_counter(set_counter), get_counter(get_counter_func), get_config(config_func), iteration_counter(counter), uses_cpu_affinity(cpu_affinity_enabled), cpu_core(assigned_cpu_core) {}
    
    // Method to set thread_function after construction
    void set_thread_function(std::function<void()> func) {
        thread_function = func;
    }
};

} // namespace ThreadSystem
} // namespace Core
} // namespace AlpacaTrader

#endif // THREAD_DEFINITION_HPP
