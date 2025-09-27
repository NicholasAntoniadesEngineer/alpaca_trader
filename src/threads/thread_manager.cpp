#include "threads/thread_manager.hpp"
#include "threads/platform/thread_control.hpp"
#include "logging/startup_logs.hpp"
#include "logging/thread_logs.hpp"
#include "core/system_threads.hpp"
#include "core/trading_system_modules.hpp"
#include "core/system_manager.hpp"
#include <iostream>

// Using declarations for cleaner code
using AlpacaTrader::Logging::AsyncLogger;
using ThreadSystem::Platform::ThreadControl;

namespace ThreadSystem {

// Static member definitions
std::vector<std::tuple<std::string, std::string, bool>> Manager::thread_status_data;
std::vector<std::thread> Manager::active_threads_;

void Manager::start_threads(const std::vector<ThreadDefinition>& thread_definitions) {

    active_threads_.clear();
    
    StartupLogs::log_thread_system_startup(TimingConfig{}); // You might want to pass actual timing config
    
    for (const auto& thread_def : thread_definitions) {
        active_threads_.emplace_back(thread_def.thread_function);
        std::cout << "Started thread: " << thread_def.name << std::endl;
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

void Manager::setup_thread_priorities(const std::vector<ThreadDefinition>& thread_definitions, const SystemConfig& config) {
    thread_status_data.clear();
    if (!config.timing.thread_priorities.enable_thread_priorities) {
        return;
    }
    
    for (const auto& thread_def : thread_definitions) {
        configure_single_thread(thread_def, config);
    }
    StartupLogs::log_thread_system_complete();
}

std::vector<ThreadLogs::ThreadInfo> Manager::create_thread_info_vector(const std::vector<ThreadDefinition>& thread_definitions) {
    std::vector<ThreadLogs::ThreadInfo> thread_infos;
    for (const auto& thread_def : thread_definitions) {
        thread_infos.emplace_back(thread_def.name, thread_def.iteration_counter);
    }
    return thread_infos;
}

std::pair<std::vector<ThreadDefinition>, std::vector<ThreadLogs::ThreadInfo>> Manager::create_thread_configurations(SystemThreads& handles, TradingSystemModules& modules) {

    auto thread_configs = SystemManager::create_thread_config_list(handles, modules);
    
    return build_thread_objects(thread_configs);
}


std::pair<std::vector<ThreadDefinition>, std::vector<ThreadLogs::ThreadInfo>> 
Manager::build_thread_objects(const std::vector<AlpacaTrader::Config::ThreadManagerConfig>& configs) {
    std::vector<ThreadDefinition> thread_definitions;
    std::vector<ThreadLogs::ThreadInfo> thread_infos;
    
    thread_definitions.reserve(configs.size());
    thread_infos.reserve(configs.size());
    
    for (const auto& config : configs) {
        thread_definitions.emplace_back(
            config.name, 
            config.thread_function, 
            config.iteration_counter, 
            config.config_type
        );
        
        thread_infos.emplace_back(
            config.name, 
            config.iteration_counter
        );
    }
    
    return std::make_pair(std::move(thread_definitions), std::move(thread_infos));
}

void Manager::configure_single_thread(const ThreadDefinition& thread_def, const SystemConfig& config) {
    // Early return if no active threads available
    if (active_threads_.empty()) {
        ThreadLogs::log_thread_configuration_skipped(thread_def.name, "no active threads");
        thread_status_data.emplace_back(thread_def.name, "NORMAL", false);
        return;
    }
    
    // Create platform configuration
    auto platform_config = create_platform_config(thread_def, config);
    
    // Apply thread configuration
    bool success = apply_thread_configuration(thread_def, platform_config, config);
    
    // Store thread status data
    thread_status_data.emplace_back(thread_def.name, "NORMAL", success);
}

AlpacaTrader::Config::ThreadConfig Manager::create_platform_config(const ThreadDefinition& thread_def, const SystemConfig& config) {
    AlpacaTrader::Config::ThreadConfig platform_config;
    platform_config.priority = AlpacaTrader::Config::Priority::NORMAL;
    
    // Determine CPU affinity
    if (thread_def.uses_cpu_affinity && thread_def.cpu_core >= 0) {
        platform_config.cpu_affinity = thread_def.cpu_core;
    } else {
        platform_config.cpu_affinity = get_default_cpu_affinity(thread_def.config_type, config);
    }
    
    return platform_config;
}

int Manager::get_default_cpu_affinity(AlpacaTrader::Config::Type thread_type, const SystemConfig& config) {
    switch (thread_type) {
        case AlpacaTrader::Config::Type::TRADER_DECISION:
            return config.timing.thread_priorities.trader_cpu_affinity;
        case AlpacaTrader::Config::Type::MARKET_DATA:
            return config.timing.thread_priorities.market_data_cpu_affinity;
        default:
            return -1; // No affinity
    }
}

bool Manager::apply_thread_configuration(const ThreadDefinition& thread_def, 
                                       const AlpacaTrader::Config::ThreadConfig& platform_config, 
                                       const SystemConfig& config) {
    // Apply platform-specific thread configuration
    bool success = ThreadControl::set_priority_with_fallback(active_threads_.back(), platform_config) >= platform_config.priority;
    
    // Log configuration result
    ThreadLogs::log_configuration_result(thread_def.name, platform_config, success);
    
    return success;
}


} // namespace ThreadSystem