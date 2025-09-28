#ifndef THREAD_REGISTRY_HPP
#define THREAD_REGISTRY_HPP

#include "configs/thread_config.hpp"
#include "configs/thread_register_config.hpp"
#include "core/system/system_modules.hpp"
#include "core/system/system_threads.hpp"
#include "configs/system_config.hpp"
#include "core/logging/thread_logs.hpp"
#include "core/logging/logging_macros.hpp"
#include "thread_definition.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <string>
#include <stdexcept>

namespace AlpacaTrader {
namespace Core {

class ThreadRegistry {
public:
    using ThreadDefinition = ThreadSystem::ThreadDefinition;
    using Type = AlpacaTrader::Config::ThreadType;
    
    struct ThreadEntry {
        Type type;
        std::string identifier;
        std::function<void(SystemModules&)> get_function;
        std::function<std::atomic<unsigned long>&(SystemThreads&)> get_counter;
        std::function<AlpacaTrader::Config::ThreadSettings(const AlpacaTrader::Config::SystemConfig&)> get_config;
        std::function<void(SystemModules&, std::atomic<unsigned long>&)> set_iteration_counter;
    };
    
    static AlpacaTrader::Config::ThreadSettings get_thread_config(Type type, const AlpacaTrader::Config::SystemConfig& system_config);
    static std::vector<ThreadDefinition> create_thread_definitions(SystemThreads& handles, SystemModules& modules);
    static std::vector<Type> create_thread_types();
    static std::vector<ThreadLogs::ThreadInfo> create_thread_infos(const std::vector<ThreadDefinition>& definitions);
    static AlpacaTrader::Config::ThreadSettings get_config_for_type(Type type, const AlpacaTrader::Config::SystemConfig& system_config);
    static void configure_thread_iteration_counters(SystemThreads& handles, SystemModules& modules);

private:
    static const std::vector<ThreadEntry> THREAD_REGISTRY;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // THREAD_REGISTRY_HPP
