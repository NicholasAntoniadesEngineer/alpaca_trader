#include "thread_logic/thread_registry.hpp"
#include "thread_logic/thread_manager.hpp"
#include "system_threads/market_data_thread.hpp"
#include "system_threads/account_data_thread.hpp"
#include "system_threads/market_gate_thread.hpp"
#include "system_threads/trader_thread.hpp"
#include "system_threads/logging_thread.hpp"
#include "logging/thread_logs.hpp"
#include "logging/logging_macros.hpp"
#include <iostream>
#include <map>
#include <set>

namespace AlpacaTrader {
namespace Core {

// =============================================================================
// THREAD REGISTER - Configuration definitions for all threads
// =============================================================================
// Each thread entry contains 3 lambda functions:
// 1. Thread execution function - safely runs the thread with error handling
// 2. Iteration counter accessor - provides access to thread's iteration counter
// 3. Configuration accessor - provides access to thread's configuration settings
// =============================================================================

const std::vector<ThreadRegistry::ThreadEntry> ThreadRegistry::THREAD_REGISTRY = {
    {
        AlpacaTrader::Config::ThreadType::MARKET_DATA,
        "MARKET_DATA",
        [](TradingSystemModules& modules) { ThreadSystem::Manager::safe_thread_execution([&modules]() { (*modules.market_data_thread)(); }, "MarketDataThread"); },
        [](SystemThreads& handles) -> std::atomic<unsigned long>& { return handles.market_iterations; },
        [](const AlpacaTrader::Config::SystemConfig& config) -> AlpacaTrader::Config::ThreadSettings { return config.thread_registry.market_data; }
    },
    {
        AlpacaTrader::Config::ThreadType::ACCOUNT_DATA,
        "ACCOUNT_DATA",
        [](TradingSystemModules& modules) { ThreadSystem::Manager::safe_thread_execution([&modules]() { (*modules.account_data_thread)(); }, "AccountDataThread"); },
        [](SystemThreads& handles) -> std::atomic<unsigned long>& { return handles.account_iterations; },
        [](const AlpacaTrader::Config::SystemConfig& config) -> AlpacaTrader::Config::ThreadSettings { return config.thread_registry.account_data; }
    },
    {
        AlpacaTrader::Config::ThreadType::MARKET_GATE,
        "MARKET_GATE",
        [](TradingSystemModules& modules) { ThreadSystem::Manager::safe_thread_execution([&modules]() { (*modules.market_gate_thread)(); }, "MarketGateThread"); },
        [](SystemThreads& handles) -> std::atomic<unsigned long>& { return handles.gate_iterations; },
        [](const AlpacaTrader::Config::SystemConfig& config) -> AlpacaTrader::Config::ThreadSettings { return config.thread_registry.market_gate; }
    },
    {
        AlpacaTrader::Config::ThreadType::TRADER_DECISION,
        "TRADER_DECISION",
        [](TradingSystemModules& modules) { ThreadSystem::Manager::safe_thread_execution([&modules]() { (*modules.trading_thread)(); }, "TraderThread"); },
        [](SystemThreads& handles) -> std::atomic<unsigned long>& { return handles.trader_iterations; },
        [](const AlpacaTrader::Config::SystemConfig& config) -> AlpacaTrader::Config::ThreadSettings { return config.thread_registry.trader_decision; }
    },
    {
        AlpacaTrader::Config::ThreadType::LOGGING,
        "LOGGING",
        [](TradingSystemModules& modules) { ThreadSystem::Manager::safe_thread_execution([&modules]() { (*modules.logging_thread)(); }, "LoggingThread"); },
        [](SystemThreads& handles) -> std::atomic<unsigned long>& { return handles.logger_iterations; },
        [](const AlpacaTrader::Config::SystemConfig& config) -> AlpacaTrader::Config::ThreadSettings { return config.thread_registry.logging; }
    }
};

} // namespace Core
} // namespace AlpacaTrader
