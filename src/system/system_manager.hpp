#ifndef SYSTEM_MANAGER_HPP
#define SYSTEM_MANAGER_HPP

#include <memory>
#include "system/system_configurations.hpp"
#include "system/system_modules.hpp"
#include "system/system_state.hpp"
#include "system/system_threads.hpp"

// Logging includes
#include "logging/logger/async_logger.hpp"

namespace AlpacaTrader {
namespace System {

struct SystemInitializationResult {
    std::unique_ptr<SystemState> system_state;
    std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger;
    
    SystemInitializationResult() = default;
    SystemInitializationResult(SystemInitializationResult&&) = default;
    SystemInitializationResult& operator=(SystemInitializationResult&&) = default;
    
    SystemInitializationResult(const SystemInitializationResult&) = delete;
    SystemInitializationResult& operator=(const SystemInitializationResult&) = delete;
};

// System initialization - handles all setup
SystemInitializationResult initialize();

// System lifecycle management
SystemThreads startup(SystemState& system_state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger);
void run(SystemState& system_state);
void shutdown(SystemState& system_state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger);

// Internal helper functions
SystemConfigurations create_trading_configurations(const SystemState& state);
SystemModules create_trading_modules(SystemState& state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger, SystemThreads& thread_handles);
void configure_trading_modules(SystemThreads& handles, SystemModules& modules, SystemState& state);

} // namespace System
} // namespace AlpacaTrader

#endif // SYSTEM_MANAGER_HPP
