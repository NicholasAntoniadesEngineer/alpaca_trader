#ifndef THREAD_MANAGER_HPP
#define THREAD_MANAGER_HPP

#include "platform/thread_control.hpp"
#include "configs/timing_config.hpp"
#include "thread_definition.hpp"
#include "thread_registry.hpp"
#include "core/logging/thread_logs.hpp"
#include "core/system/system_threads.hpp"
#include "core/system/system_modules.hpp"
#include "configs/system_config.hpp"
#include <vector>
#include <tuple>
#include <string>
#include <thread>
#include <memory>
#include <functional>
#include <atomic>

namespace AlpacaTrader {
namespace Core {
namespace ThreadSystem {

// High-level thread management interface
class Manager {
public:
    // Thread lifecycle management
    static void start_threads(const std::vector<AlpacaTrader::Core::ThreadSystem::ThreadDefinition>& thread_definitions, SystemModules& modules);
    static void shutdown_threads();
    
    // Thread monitoring and logging
    static void log_thread_monitoring_stats(const std::vector<ThreadLogs::ThreadInfo>& thread_infos, 
                                          const std::chrono::steady_clock::time_point& start_time);
    
    // Thread priority management
    static void setup_thread_priorities(const std::vector<AlpacaTrader::Core::ThreadSystem::ThreadDefinition>& thread_definitions, const AlpacaTrader::Config::SystemConfig& config);
    
    
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
    static void configure_single_thread(const AlpacaTrader::Core::ThreadSystem::ThreadDefinition& thread_def, AlpacaTrader::Core::ThreadRegistry::Type thread_type, const AlpacaTrader::Config::SystemConfig& config);
    static AlpacaTrader::Config::ThreadSettings create_platform_config(const AlpacaTrader::Core::ThreadSystem::ThreadDefinition& thread_def, AlpacaTrader::Core::ThreadRegistry::Type thread_type, const AlpacaTrader::Config::SystemConfig& config);
    static bool apply_thread_configuration(const AlpacaTrader::Config::ThreadSettings& platform_config);
    
    
    // Thread status tracking
    static std::vector<AlpacaTrader::Config::ThreadStatusData> thread_status_data;
    static std::vector<std::thread> active_threads_;
};

} // namespace ThreadSystem
} // namespace Core
} // namespace AlpacaTrader

#endif // THREAD_MANAGER_HPP