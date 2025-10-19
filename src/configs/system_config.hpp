#ifndef SYSTEM_CONFIG_HPP
#define SYSTEM_CONFIG_HPP

#include "strategy_config.hpp"
#include "timing_config.hpp"
#include "logging_config.hpp"
#include "multi_api_config.hpp"
#include "trading_mode_config.hpp"
#include "core/threads/thread_register.hpp"

namespace AlpacaTrader {
namespace Config {

/**
 * Main trading system configuration.
 * Strategy config includes: strategy, risk, position, profit-taking, orders, target, session, monitoring, error handling
 * Timing config includes: timing and system health monitoring
 */
struct SystemConfig {
    // Default constructor - ensures nested structs are properly constructed
    SystemConfig() {}

    StrategyConfig strategy;           // All strategy-related settings (strategy, risk, position, profit-taking, orders, target, session, monitoring, error handling)
    TimingConfig timing;               // All timing and polling intervals
    LoggingConfig logging;             // Logging configuration
    MultiApiConfig multi_api;          // Multi-provider API configuration
    TradingModeConfig trading_mode;    // Trading mode configuration (stocks vs crypto)
    ThreadConfigRegistry thread_registry; // Thread priorities and CPU affinity
};

} // namespace Config
} // namespace AlpacaTrader

#endif // SYSTEM_CONFIG_HPP
