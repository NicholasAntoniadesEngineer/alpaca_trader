#include "threads/thread_manager.hpp"
#include "threads/platform/thread_control.hpp"
#include "logging/startup_logger.hpp"
#include "logging/thread_logger.hpp"
#include "core/system_threads.hpp"
#include "core/trading_system_modules.hpp"
#include <iostream>

// Using declarations for cleaner code
using AlpacaTrader::Logging::log_message;
using AlpacaTrader::Logging::AsyncLogger;
using ThreadSystem::Platform::ThreadControl;

namespace ThreadSystem {

// Static member definitions
std::vector<std::tuple<std::string, std::string, bool>> Manager::thread_status_data;
std::vector<std::thread> Manager::active_threads_;

void Manager::start_threads(const std::vector<ThreadDefinition>& thread_definitions) {
    // Clear any existing threads
    active_threads_.clear();
    
    // Log thread system startup
    StartupLogger::log_thread_system_startup(TimingConfig{}); // You might want to pass actual timing config
    
    // Start all threads
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

void Manager::log_thread_monitoring_stats(const std::vector<ThreadLogger::ThreadInfo>& thread_infos, 
                                        const std::chrono::steady_clock::time_point& start_time) {
    ThreadLogger::log_thread_monitoring_stats(thread_infos, start_time);
}

void Manager::setup_thread_priorities(const std::vector<ThreadDefinition>& thread_definitions, const SystemConfig& config) {
    thread_status_data.clear();
    if (!config.timing.thread_priorities.enable_thread_priorities) {
        return;
    }
    
    for (const auto& thread_def : thread_definitions) {
        configure_single_thread(thread_def, config);
    }
    StartupLogger::log_thread_system_complete();
}

std::vector<ThreadLogger::ThreadInfo> Manager::create_thread_info_vector(const std::vector<ThreadDefinition>& thread_definitions) {
    std::vector<ThreadLogger::ThreadInfo> thread_infos;
    for (const auto& thread_def : thread_definitions) {
        thread_infos.emplace_back(thread_def.name, thread_def.iteration_counter);
    }
    return thread_infos;
}

std::pair<std::vector<ThreadDefinition>, std::vector<ThreadLogger::ThreadInfo>> Manager::create_thread_configurations(SystemThreads& handles, TradingSystemModules& modules) {
    // Create thread configuration list
    auto thread_configs = create_thread_config_list(handles, modules);
    
    // Build thread objects from configuration
    return build_thread_objects(thread_configs);
}

std::vector<AlpacaTrader::Config::ThreadManagerConfig> Manager::create_thread_config_list(SystemThreads& handles, TradingSystemModules& modules) {
    std::vector<AlpacaTrader::Config::ThreadManagerConfig> configs;
    configs.reserve(5);
    
    // Market data processing thread
    configs.emplace_back(
        "Market Thread",
        [&modules]() { (*modules.market_data_thread)(); },
        handles.market_iterations,
        AlpacaTrader::Config::Type::MARKET_DATA
    );
    
    // Account data processing thread
    configs.emplace_back(
        "Account Thread",
        [&modules]() { (*modules.account_data_thread)(); },
        handles.account_iterations,
        AlpacaTrader::Config::Type::ACCOUNT_DATA
    );
    
    // Market gate control thread
    configs.emplace_back(
        "Gate Thread",
        [&modules]() { (*modules.market_gate_thread)(); },
        handles.gate_iterations,
        AlpacaTrader::Config::Type::MARKET_GATE
    );
    
    // Main trading logic thread
    configs.emplace_back(
        "Trader Thread",
        [&modules]() { (*modules.trading_thread)(); },
        handles.trader_iterations,
        AlpacaTrader::Config::Type::TRADER_DECISION
    );
    
    // Logging system thread
    configs.emplace_back(
        "Logger Thread",
        [&modules]() { (*modules.logging_thread)(); },
        handles.logger_iterations,
        AlpacaTrader::Config::Type::LOGGING
    );
    
    return configs;
}

std::pair<std::vector<ThreadDefinition>, std::vector<ThreadLogger::ThreadInfo>> 
Manager::build_thread_objects(const std::vector<AlpacaTrader::Config::ThreadManagerConfig>& configs) {
    std::vector<ThreadDefinition> thread_definitions;
    std::vector<ThreadLogger::ThreadInfo> thread_infos;
    
    thread_definitions.reserve(configs.size());
    thread_infos.reserve(configs.size());
    
    // Generate both thread definitions and thread infos from the same configuration
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
        log_message("Thread " + thread_def.name + " configuration skipped - no active threads", 
                   config.logging.log_file);
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
    log_configuration_result(thread_def, platform_config, success, config);
    
    return success;
}

void Manager::log_configuration_result(const ThreadDefinition& thread_def,
                                     const AlpacaTrader::Config::ThreadConfig& platform_config,
                                     bool success,
                                     const SystemConfig& config) {
    if (success) {
        if (platform_config.cpu_affinity >= 0) {
            log_message("Thread " + thread_def.name + " configured for CPU core " + 
                       std::to_string(platform_config.cpu_affinity) + " with priority NORMAL", 
                       config.logging.log_file);
        } else {
            log_message("Thread " + thread_def.name + " configured with priority NORMAL", 
                       config.logging.log_file);
        }
    } else {
        if (platform_config.cpu_affinity >= 0) {
            log_message("Thread " + thread_def.name + " CPU affinity configuration failed, using fallback priority", 
                       config.logging.log_file);
        } else {
            log_message("Thread " + thread_def.name + " priority configuration failed", 
                       config.logging.log_file);
        }
    }
}

} // namespace ThreadSystem