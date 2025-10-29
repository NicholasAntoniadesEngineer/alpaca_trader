#include "thread_registry.hpp"
#include "thread_manager.hpp"
#include "core/logging/logs/thread_logs.hpp"
#include "core/logging/logger/logging_macros.hpp"
#include <iostream>

namespace AlpacaTrader {
namespace Core {

AlpacaTrader::Config::ThreadSettings ThreadRegistry::get_thread_config(Type type, const AlpacaTrader::Config::SystemConfig& system_config) {
    // Handle MAIN thread separately as it's not in the registry
    if (type == AlpacaTrader::Config::ThreadType::MAIN) {
        return system_config.thread_registry.get_thread_settings("main");
    }
    
    // Find the thread in the registry
    for (const auto& entry : THREAD_REGISTRY) {
        if (entry.type == type) {
            return entry.get_config(system_config);
        }
    }

    std::string type_name = "UNKNOWN_TYPE_" + std::to_string(static_cast<int>(type));
    std::string error_msg = ThreadLogs::build_unknown_thread_type_error(type_name, static_cast<int>(type));
    ThreadLogs::log_thread_registry_error(error_msg);
    throw std::runtime_error("ThreadRegistry::get_thread_config - " + error_msg);
}

std::vector<ThreadRegistry::Type> ThreadRegistry::create_thread_types() {
    std::vector<Type> types;
    types.reserve(THREAD_REGISTRY.size());
    
    for (const auto& entry : THREAD_REGISTRY) {
        types.push_back(entry.type);
    }
    
    return types;
}

std::vector<ThreadRegistry::ThreadDefinition> ThreadRegistry::create_thread_definitions(SystemThreads& handles, SystemModules& modules) {
    std::vector<ThreadDefinition> definitions;
    definitions.reserve(THREAD_REGISTRY.size());
    
    for (const auto& entry : THREAD_REGISTRY) {
        ThreadDefinition def(
            entry.identifier,
            entry.get_function,
            entry.set_iteration_counter,
            entry.get_counter,
            entry.get_config,
            entry.get_counter(handles)
        );
        // Set up the thread function with the modules parameter
        def.set_thread_function([&entry, &modules]() { 
            entry.get_function(modules); 
        });
        definitions.push_back(def);
    }
    
    return definitions;
}

std::vector<ThreadLogs::ThreadInfo> ThreadRegistry::create_thread_infos(const std::vector<ThreadDefinition>& definitions) {
    std::vector<ThreadLogs::ThreadInfo> infos;
    infos.reserve(definitions.size());
    
    for (const auto& def : definitions) {
        infos.emplace_back(def.name, def.iteration_counter);
    }
    
    return infos;
}

AlpacaTrader::Config::ThreadSettings ThreadRegistry::get_config_for_type(Type type, const AlpacaTrader::Config::SystemConfig& system_config) {
    return get_thread_config(type, system_config);
}

void ThreadRegistry::configure_thread_iteration_counters(SystemThreads& handles, SystemModules& modules) {
    for (const auto& entry : THREAD_REGISTRY) {
        if (entry.set_iteration_counter) {
            entry.set_iteration_counter(modules, entry.get_counter(handles));
        }
    }
}



} // namespace Core
} // namespace AlpacaTrader
