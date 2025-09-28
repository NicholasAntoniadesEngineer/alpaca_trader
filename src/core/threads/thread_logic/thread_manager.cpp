#include "thread_manager.hpp"
#include "platform/thread_control.hpp"
#include "core/logging/startup_logs.hpp"
#include "core/logging/thread_logs.hpp"
#include "core/system/system_threads.hpp"
#include "core/system/system_modules.hpp"
#include "thread_registry.hpp"

using ThreadSystem::Platform::ThreadControl;

namespace AlpacaTrader {
namespace Core {
namespace ThreadSystem {

std::vector<AlpacaTrader::Config::ThreadStatusData> Manager::thread_status_data;
std::vector<std::thread> Manager::active_threads_;

void Manager::start_threads(const std::vector<AlpacaTrader::Core::ThreadSystem::ThreadDefinition>& thread_definitions, SystemModules& modules) 
{
    active_threads_.clear();
   
    for (const auto& thread_def : thread_definitions) {
        // Use get_function directly with modules
        active_threads_.emplace_back([&thread_def, &modules]() { 
            thread_def.get_function(modules);
        });
    }
}

void Manager::shutdown_threads() {
    for (auto& thread : active_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    active_threads_.clear();
}

void Manager::log_thread_monitoring_stats(const std::vector<ThreadLogs::ThreadInfo>& thread_infos, 
                                        const std::chrono::steady_clock::time_point& start_time) {
    ThreadLogs::log_thread_monitoring_stats(thread_infos, start_time);
}

void Manager::setup_thread_priorities(const std::vector<AlpacaTrader::Core::ThreadSystem::ThreadDefinition>& thread_definitions, const AlpacaTrader::Config::SystemConfig& config) 
{
    thread_status_data.clear();
    if (!config.timing.thread_priorities.enable_thread_priorities) {
        return;
    }
    
    auto thread_types = AlpacaTrader::Core::ThreadRegistry::create_thread_types();
    
    for (size_t i = 0; i < thread_definitions.size() && i < thread_types.size(); ++i) {
        configure_single_thread(thread_definitions[i], thread_types[i], config);
    }
    ThreadLogs::log_thread_status_table(thread_status_data);    
}

void Manager::configure_single_thread(const AlpacaTrader::Core::ThreadSystem::ThreadDefinition& thread_def, AlpacaTrader::Core::ThreadRegistry::Type thread_type, const AlpacaTrader::Config::SystemConfig& config) 
{    
    if (active_threads_.empty()) {
        thread_status_data.emplace_back(thread_def.name, "SKIPPED", false, -1);
        return;
    }
    
    auto platform_config = create_platform_config(thread_def, thread_type, config);
    bool success = apply_thread_configuration(thread_def, platform_config, config);
    
    std::string priority_str = AlpacaTrader::Config::ConfigProvider::priority_to_string(static_cast<AlpacaTrader::Config::Priority>(platform_config.priority));
    std::string cpu_info = (platform_config.cpu_affinity >= 0) ? "CPU " + std::to_string(platform_config.cpu_affinity) : "No affinity";
    std::string status_msg = success ? "Configured" : "Failed";
    thread_status_data.emplace_back(thread_def.name, priority_str, success, platform_config.cpu_affinity, status_msg);
}

AlpacaTrader::Config::ThreadSettings Manager::create_platform_config(const AlpacaTrader::Core::ThreadSystem::ThreadDefinition& thread_def, AlpacaTrader::Core::ThreadRegistry::Type thread_type, const AlpacaTrader::Config::SystemConfig& config) 
{
    auto platform_config = AlpacaTrader::Core::ThreadRegistry::get_config_for_type(thread_type, config);
    
    if (thread_def.uses_cpu_affinity && thread_def.cpu_core >= 0) {
        platform_config.cpu_affinity = thread_def.cpu_core;
    }
    
    return platform_config;
}

bool Manager::apply_thread_configuration(const AlpacaTrader::Core::ThreadSystem::ThreadDefinition& /*thread_def*/, 
                                       const AlpacaTrader::Config::ThreadSettings& platform_config, 
                                       const AlpacaTrader::Config::SystemConfig& /*config*/) 
{
    AlpacaTrader::Config::Priority actual_priority = ThreadControl::set_priority_with_fallback(active_threads_.back(), platform_config);
    bool success = (actual_priority == platform_config.priority);
    
    return success;
}

} // namespace ThreadSystem
} // namespace Core
} // namespace AlpacaTrader