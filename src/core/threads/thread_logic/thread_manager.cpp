#include "thread_manager.hpp"
#include "platform/thread_control.hpp"
#include "core/logging/logs/startup_logs.hpp"
#include "core/logging/logs/thread_logs.hpp"
#include "core/system/system_threads.hpp"
#include "core/system/system_modules.hpp"
#include "thread_registry.hpp"

using ThreadSystem::Platform::ThreadControl;

namespace AlpacaTrader {
namespace Core {

void Manager::start_threads(ThreadManagerState& manager_state, const std::vector<AlpacaTrader::Core::ThreadSystem::ThreadDefinition>& thread_definitions, SystemModules& modules) 
{
    manager_state.clear_all_data();
   
    for (const auto& thread_definition : thread_definitions) {
        // Capture thread_definition by value to avoid dangling reference
        manager_state.add_active_thread(std::thread([thread_definition, &modules]() {
            thread_definition.get_function(modules);
        }));
    }
}

void Manager::shutdown_threads(ThreadManagerState& manager_state) {
    for (auto& thread_instance : manager_state.active_threads) {
        if (thread_instance.joinable()) {
            thread_instance.join();
        }
    }
    manager_state.clear_all_data();
}

void Manager::setup_thread_priorities(ThreadManagerState& manager_state, const std::vector<AlpacaTrader::Core::ThreadSystem::ThreadDefinition>& thread_definitions, const AlpacaTrader::Config::SystemConfig& config) 
{
    manager_state.thread_status_data.clear();
    
    auto thread_types = AlpacaTrader::Core::ThreadRegistry::create_thread_types();
    
    for (size_t thread_index = 0; thread_index < thread_definitions.size() && thread_index < thread_types.size(); ++thread_index) {
        configure_single_thread(manager_state, thread_definitions[thread_index], thread_types[thread_index], config);
    }
    ThreadLogs::log_thread_status_table(manager_state.thread_status_data);    
}

void Manager::configure_single_thread(ThreadManagerState& manager_state, const AlpacaTrader::Core::ThreadSystem::ThreadDefinition& thread_def, AlpacaTrader::Core::ThreadRegistry::Type thread_type, const AlpacaTrader::Config::SystemConfig& config) 
{    
    if (!manager_state.has_active_threads()) {
        manager_state.add_thread_status(AlpacaTrader::Config::ThreadStatusData(thread_def.name, "SKIPPED", false, -1));
        return;
    }
    
    auto platform_config = create_platform_config(thread_def, thread_type, config);
    bool configuration_success = apply_thread_configuration(manager_state, platform_config);
    
    std::string priority_string = AlpacaTrader::Config::ConfigProvider::priority_to_string(static_cast<AlpacaTrader::Config::Priority>(platform_config.priority));
    std::string cpu_affinity_info = (platform_config.cpu_affinity >= 0) ? "CPU " + std::to_string(platform_config.cpu_affinity) : "No affinity";
    std::string status_message = configuration_success ? "Configured" : "Failed";
    manager_state.add_thread_status(AlpacaTrader::Config::ThreadStatusData(thread_def.name, priority_string, configuration_success, platform_config.cpu_affinity, status_message));
}

AlpacaTrader::Config::ThreadSettings Manager::create_platform_config(const AlpacaTrader::Core::ThreadSystem::ThreadDefinition& thread_def, AlpacaTrader::Core::ThreadRegistry::Type thread_type, const AlpacaTrader::Config::SystemConfig& config) 
{
    auto platform_config = AlpacaTrader::Core::ThreadRegistry::get_config_for_type(thread_type, config);
    
    if (thread_def.uses_cpu_affinity && thread_def.cpu_core >= 0) {
        platform_config.cpu_affinity = thread_def.cpu_core;
    }
    
    return platform_config;
}

bool Manager::apply_thread_configuration(ThreadManagerState& manager_state, const AlpacaTrader::Config::ThreadSettings& platform_config) 
{
    AlpacaTrader::Config::Priority actual_priority = ThreadControl::set_priority_with_fallback(manager_state.get_last_thread(), platform_config);
    bool configuration_success = (actual_priority == platform_config.priority);
    
    return configuration_success;
}

} // namespace Core
} // namespace AlpacaTrader