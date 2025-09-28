#ifndef THREAD_MANAGER_HPP
#define THREAD_MANAGER_HPP

#include "platform/thread_control.hpp"
#include "configs/timing_config.hpp"
#include "thread_registry.hpp"
#include "logging/thread_logs.hpp"
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

// Generic thread definition for thread management
struct ThreadDefinition {
    std::string name;
    std::function<void()> thread_function;
    std::atomic<unsigned long>& iteration_counter;
    bool uses_cpu_affinity;
    int cpu_core;
    
    ThreadDefinition(const std::string& n, std::function<void()> func, 
                    std::atomic<unsigned long>& counter,
                    bool affinity = false, int core = -1)
        : name(n), thread_function(func), iteration_counter(counter), 
          uses_cpu_affinity(affinity), cpu_core(core) {}
};


// High-level thread management interface
class Manager {
public:
    // Thread lifecycle management
    static void start_threads(const std::vector<ThreadDefinition>& thread_definitions);
    static void shutdown_threads();
    
    // Thread monitoring and logging
    static void log_thread_monitoring_stats(const std::vector<ThreadLogs::ThreadInfo>& thread_infos, 
                                          const std::chrono::steady_clock::time_point& start_time);
    
    // Thread priority management
    static void setup_thread_priorities(const std::vector<ThreadDefinition>& thread_definitions, const AlpacaTrader::Config::SystemConfig& config);
    
    // Utility functions
    static std::vector<ThreadLogs::ThreadInfo> create_thread_info_vector(const std::vector<ThreadDefinition>& thread_definitions);
    static std::pair<std::vector<ThreadDefinition>, std::vector<ThreadLogs::ThreadInfo>> create_thread_configurations(SystemThreads& handles, TradingSystemModules& modules);
    
    // Exception-safe thread execution
    template<typename ThreadFunction>
    static void safe_thread_execution(ThreadFunction&& thread_func, const std::string& thread_name) {
        try {
            thread_func();
        } catch (const std::exception& e) {
            ThreadLogs::log_thread_exception(thread_name, std::string(e.what()));
        } catch (...) {
            ThreadLogs::log_thread_unknown_exception(thread_name);
        }
    }
    
    
private:
    // Thread setup utilities
    static void configure_single_thread(const ThreadDefinition& thread_def, AlpacaTrader::Core::ThreadRegistry::Type thread_type, const AlpacaTrader::Config::SystemConfig& config);
    static AlpacaTrader::Config::ThreadSettings create_platform_config(const ThreadDefinition& thread_def, AlpacaTrader::Core::ThreadRegistry::Type thread_type, const AlpacaTrader::Config::SystemConfig& config);
    static bool apply_thread_configuration(const ThreadDefinition& thread_def, 
                                          const AlpacaTrader::Config::ThreadSettings& platform_config, 
                                          const AlpacaTrader::Config::SystemConfig& config);
    
    
    // Thread status tracking
    static std::vector<AlpacaTrader::Config::ThreadStatusData> thread_status_data;
    static std::vector<std::thread> active_threads_;
};

} // namespace ThreadSystem

#endif // THREAD_MANAGER_HPP