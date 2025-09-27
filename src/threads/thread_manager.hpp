#ifndef THREAD_MANAGER_HPP
#define THREAD_MANAGER_HPP

#include "configs/thread_config.hpp"
#include "threads/platform/thread_control.hpp"
#include "configs/timing_config.hpp"
#include "logging/thread_logger.hpp"
#include <vector>
#include <tuple>
#include <string>
#include <thread>
#include <memory>
#include <functional>
#include <atomic>

// Forward declarations
struct SystemThreads;
struct TradingSystemModules;
struct SystemConfig;

namespace ThreadSystem {

// Thread definition for generic thread management
struct ThreadDefinition {
    std::string name;
    std::function<void()> thread_function;
    std::atomic<unsigned long>& iteration_counter;
    AlpacaTrader::Config::Type config_type;
    bool uses_cpu_affinity;
    int cpu_core;
    
    ThreadDefinition(const std::string& n, std::function<void()> func, 
                    std::atomic<unsigned long>& counter, AlpacaTrader::Config::Type type,
                    bool affinity = false, int core = -1)
        : name(n), thread_function(func), iteration_counter(counter), 
          config_type(type), uses_cpu_affinity(affinity), cpu_core(core) {}
};

// High-level thread management interface
class Manager {
public:
    // Thread lifecycle management
    static void start_threads(const std::vector<ThreadDefinition>& thread_definitions);
    static void shutdown_threads();
    
    // Thread monitoring and logging
    static void log_thread_monitoring_stats(const std::vector<ThreadLogger::ThreadInfo>& thread_infos, 
                                          const std::chrono::steady_clock::time_point& start_time);
    
    // Thread priority management
    static void setup_thread_priorities(const std::vector<ThreadDefinition>& thread_definitions, const SystemConfig& config);
    
    // Utility functions
    static std::vector<ThreadLogger::ThreadInfo> create_thread_info_vector(const std::vector<ThreadDefinition>& thread_definitions);
    static std::pair<std::vector<ThreadDefinition>, std::vector<ThreadLogger::ThreadInfo>> create_thread_configurations(SystemThreads& handles, TradingSystemModules& modules);
    
    
private:
    // Thread setup utilities
    static void configure_single_thread(const ThreadDefinition& thread_def, const SystemConfig& config);
    static AlpacaTrader::Config::ThreadConfig create_platform_config(const ThreadDefinition& thread_def, const SystemConfig& config);
    static int get_default_cpu_affinity(AlpacaTrader::Config::Type thread_type, const SystemConfig& config);
    static bool apply_thread_configuration(const ThreadDefinition& thread_def, 
                                          const AlpacaTrader::Config::ThreadConfig& platform_config, 
                                          const SystemConfig& config);
    static void log_configuration_result(const ThreadDefinition& thread_def,
                                        const AlpacaTrader::Config::ThreadConfig& platform_config,
                                        bool success,
                                        const SystemConfig& config);
    
    // Thread configuration helpers
    static std::vector<AlpacaTrader::Config::ThreadManagerConfig> create_thread_config_list(SystemThreads& handles, TradingSystemModules& modules);
    static std::pair<std::vector<ThreadDefinition>, std::vector<ThreadLogger::ThreadInfo>> 
        build_thread_objects(const std::vector<AlpacaTrader::Config::ThreadManagerConfig>& configs);
    
    // Thread status tracking
    static std::vector<std::tuple<std::string, std::string, bool>> thread_status_data;
    static std::vector<std::thread> active_threads_;
};

} // namespace ThreadSystem

#endif // THREAD_MANAGER_HPP