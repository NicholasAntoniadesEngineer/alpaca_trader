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

// Thread manager state structure to avoid global variables
struct ThreadManagerState {
    std::vector<AlpacaTrader::Config::ThreadStatusData> thread_status_data;
    std::vector<std::thread> active_threads;
    
    ThreadManagerState() = default;
    
    void clear_all_data() {
        thread_status_data.clear();
        active_threads.clear();
    }
    
    void add_thread_status(const AlpacaTrader::Config::ThreadStatusData& status_data) {
        thread_status_data.push_back(status_data);
    }
    
    void add_active_thread(std::thread&& thread_instance) {
        active_threads.push_back(std::move(thread_instance));
    }
    
    bool has_active_threads() const {
        return !active_threads.empty();
    }
    
    std::thread& get_last_thread() {
        return active_threads.back();
    }
};

// High-level thread management interface
class Manager {
public:
    // Thread lifecycle management
    static void start_threads(ThreadManagerState& manager_state, const std::vector<AlpacaTrader::Core::ThreadSystem::ThreadDefinition>& thread_definitions, SystemModules& modules);
    static void shutdown_threads(ThreadManagerState& manager_state);
    
    // Thread monitoring and logging
    static void log_thread_monitoring_stats(const std::vector<ThreadLogs::ThreadInfo>& thread_infos, 
                                          const std::chrono::steady_clock::time_point& start_time);
    
    // Thread priority management
    static void setup_thread_priorities(ThreadManagerState& manager_state, const std::vector<AlpacaTrader::Core::ThreadSystem::ThreadDefinition>& thread_definitions, const AlpacaTrader::Config::SystemConfig& config);
    
    
    // Exception-safe thread execution
    template<typename ThreadFunction>
    static void safe_thread_execution(ThreadFunction&& thread_func, const std::string& thread_name) {
        try {
            thread_func();
        } catch (const std::exception& exception) {
            ThreadLogs::log_thread_exception(thread_name, std::string(exception.what()));
        } catch (...) {
            ThreadLogs::log_thread_unknown_exception(thread_name);
        }
    }  
    
private:
    // Thread setup utilities
    static void configure_single_thread(ThreadManagerState& manager_state, const AlpacaTrader::Core::ThreadSystem::ThreadDefinition& thread_def, AlpacaTrader::Core::ThreadRegistry::Type thread_type, const AlpacaTrader::Config::SystemConfig& config);
    static AlpacaTrader::Config::ThreadSettings create_platform_config(const AlpacaTrader::Core::ThreadSystem::ThreadDefinition& thread_def, AlpacaTrader::Core::ThreadRegistry::Type thread_type, const AlpacaTrader::Config::SystemConfig& config);
    static bool apply_thread_configuration(ThreadManagerState& manager_state, const AlpacaTrader::Config::ThreadSettings& platform_config);
};

} // namespace ThreadSystem
} // namespace Core
} // namespace AlpacaTrader

#endif // THREAD_MANAGER_HPP