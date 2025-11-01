// system_manager.hpp
#ifndef SYSTEM_MANAGER_HPP
#define SYSTEM_MANAGER_HPP

#include <mutex>
#include <memory>
#include <vector>
#include "logging/logger/async_logger.hpp"
#include "configs/thread_config.hpp"
#include "system_state.hpp"
#include "system_threads.hpp"
#include "system_modules.hpp"
#include "system_configurations.hpp"

namespace SystemManager {

// System lifecycle management
SystemThreads startup(SystemState& system_state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger);
void run(SystemState& system_state);
void shutdown(SystemState& system_state, std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger);


} // namespace SystemManager

#endif // SYSTEM_MANAGER_HPP
