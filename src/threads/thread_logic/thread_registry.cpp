#include "thread_registry.hpp"
#include "thread_manager.hpp"
#include "logging/thread_logs.hpp"
#include "logging/logging_macros.hpp"
#include <iostream>
#include <map>
#include <set>

namespace AlpacaTrader {
namespace Core {

AlpacaTrader::Config::ThreadSettings ThreadRegistry::get_thread_config(Type type, const AlpacaTrader::Config::SystemConfig& system_config) {
    // Handle MAIN thread separately as it's not in the registry
    if (type == AlpacaTrader::Config::ThreadType::MAIN) {
        return system_config.thread_registry.main;
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

std::vector<ThreadRegistry::ThreadDefinition> ThreadRegistry::create_thread_definitions(SystemThreads& handles, TradingSystemModules& modules) {
    std::vector<ThreadDefinition> definitions;
    definitions.reserve(THREAD_REGISTRY.size());
    
    for (const auto& entry : THREAD_REGISTRY) {
        definitions.emplace_back(
            entry.identifier + " Thread",  // Display name
            [&modules, &entry]() { entry.get_function(modules); },
            entry.get_counter(handles),
            false,  
            -1  
        );
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

void ThreadRegistry::configure_thread_iteration_counters(SystemThreads& handles, TradingSystemModules& modules) {
    for (const auto& entry : THREAD_REGISTRY) {
        if (entry.set_iteration_counter) {
            entry.set_iteration_counter(modules, entry.get_counter(handles));
        }
    }
}

ThreadRegistry::Type ThreadRegistry::string_to_type(const std::string& identifier) {
    for (const auto& entry : THREAD_REGISTRY) {
        if (entry.identifier == identifier) {
            return entry.type;
        }
    }
    
    std::string error_msg = ThreadLogs::build_unknown_thread_identifier_error(identifier);
    ThreadLogs::log_thread_registry_error(error_msg);
    throw std::runtime_error("ThreadRegistry::string_to_type - " + error_msg);
}

std::string ThreadRegistry::type_to_string(Type type) {
    for (const auto& entry : THREAD_REGISTRY) {
        if (entry.type == type) {
            return entry.identifier;
        }
    }
    
    std::string error_msg = ThreadLogs::build_unknown_thread_type_enum_error(static_cast<int>(type));
    ThreadLogs::log_thread_registry_error(error_msg);
    throw std::runtime_error("ThreadRegistry::type_to_string - " + error_msg);
}

void ThreadRegistry::validate_registry_consistency() {
    // Check for duplicate types
    std::set<Type> seen_types;
    std::set<std::string> seen_identifiers;
    
    for (const auto& entry : THREAD_REGISTRY) {
        // Check for duplicate types
        if (seen_types.find(entry.type) != seen_types.end()) {
            std::string error_msg = ThreadLogs::build_duplicate_thread_type_error(static_cast<int>(entry.type));
            ThreadLogs::log_thread_registry_error(error_msg);
            throw std::runtime_error("ThreadRegistry::validate_registry_consistency - " + error_msg);
        }
        seen_types.insert(entry.type);
        
        // Check for duplicate identifiers
        if (seen_identifiers.find(entry.identifier) != seen_identifiers.end()) {
            std::string error_msg = ThreadLogs::build_duplicate_thread_identifier_error(entry.identifier);
            ThreadLogs::log_thread_registry_error(error_msg);
            throw std::runtime_error("ThreadRegistry::validate_registry_consistency - " + error_msg);
        }
        seen_identifiers.insert(entry.identifier);
        
        // Check for empty identifier
        if (entry.identifier.empty()) {
            std::string error_msg = ThreadLogs::build_empty_thread_identifier_error(static_cast<int>(entry.type));
            ThreadLogs::log_thread_registry_error(error_msg);
            throw std::runtime_error("ThreadRegistry::validate_registry_consistency - " + error_msg);
        }
    }
    
    // Log successful validation
    ThreadLogs::log_thread_registry_validation_success("ThreadRegistry validation completed successfully - " + 
                                                      std::to_string(THREAD_REGISTRY.size()) + " threads registered");
}


} // namespace Core
} // namespace AlpacaTrader
