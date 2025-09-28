#ifndef THREAD_REGISTRY_HPP
#define THREAD_REGISTRY_HPP

#include "configs/thread_config.hpp"
#include "configs/thread_register_config.hpp"
#include "core/trading_system_modules.hpp"
#include "core/system_threads.hpp"
#include "configs/system_config.hpp"
#include "logging/thread_logs.hpp"
#include "logging/logging_macros.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <string>
#include <stdexcept>

// Forward declaration
namespace ThreadSystem {
struct ThreadDefinition;
}

namespace AlpacaTrader {
namespace Core {

class ThreadRegistry {
public:
    using ThreadDefinition = ThreadSystem::ThreadDefinition;
    
    // Thread types - single source of truth
    // Use ThreadType from thread_register_config.hpp
    using Type = AlpacaTrader::Config::ThreadType;
    
    // Unified thread registry entry
    struct ThreadEntry {
        Type type;
        std::string identifier;
        std::function<void(TradingSystemModules&)> get_function;
        std::function<std::atomic<unsigned long>&(SystemThreads&)> get_counter;
        std::function<AlpacaTrader::Config::ThreadSettings(const AlpacaTrader::Config::SystemConfig&)> get_config;
        std::function<void(TradingSystemModules&, std::atomic<unsigned long>&)> set_iteration_counter;
    };
    
    static AlpacaTrader::Config::ThreadSettings get_thread_config(Type type, const AlpacaTrader::Config::SystemConfig& system_config);
    static std::vector<ThreadDefinition> create_thread_definitions(SystemThreads& handles, TradingSystemModules& modules);
    static std::vector<Type> create_thread_types();
    static std::vector<ThreadLogs::ThreadInfo> create_thread_infos(const std::vector<ThreadDefinition>& definitions);
    static AlpacaTrader::Config::ThreadSettings get_config_for_type(Type type, const AlpacaTrader::Config::SystemConfig& system_config);
    static void configure_thread_iteration_counters(SystemThreads& handles, TradingSystemModules& modules);

private:
    // Single source of truth - add new threads here only
    static const std::vector<ThreadEntry> THREAD_REGISTRY;
    
    // Helper methods
    static Type string_to_type(const std::string& identifier);
    static std::string type_to_string(Type type);
    
    // Error handling and validation
    static void validate_registry_consistency();
};

} // namespace Core
} // namespace AlpacaTrader

#endif // THREAD_REGISTRY_HPP
