// system_manager.hpp
#ifndef SYSTEM_MANAGER_HPP
#define SYSTEM_MANAGER_HPP

#include <mutex>
#include <memory>
#include <vector>
#include "logging/async_logger.hpp"
#include "configs/thread_config.hpp"

// Forward declarations
struct SystemState;
struct SystemThreads;
struct TradingSystemModules;

namespace SystemManager {

// System lifecycle management
SystemThreads startup(SystemState& system_state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger);
void run(SystemState& system_state, SystemThreads& handles);
void shutdown(SystemState& system_state, SystemThreads& handles, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger);

} // namespace SystemManager

#endif // SYSTEM_MANAGER_HPP
