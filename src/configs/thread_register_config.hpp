#pragma once

#include "core/threads/thread_logic/thread_types.hpp"
#include "strategy_config.hpp"
#include "timing_config.hpp"
#include "target_config.hpp"
#include "api_config.hpp"
#include "session_config.hpp"
#include "logging_config.hpp"
#include "orders_config.hpp"

namespace AlpacaTrader {
namespace Config {

// Thread types enum - single source of truth for all thread types
enum class ThreadType {
    MAIN,
    TRADER_DECISION,
    MARKET_DATA,
    ACCOUNT_DATA,
    MARKET_GATE,
    LOGGING
};

// Config types for thread components
struct AlpacaClientConfig {
    const ApiConfig& api;
    const SessionConfig& session;
    const LoggingConfig& logging;
    const TargetConfig& target;
    const TimingConfig& timing;
    const OrdersConfig& orders;
};

struct AccountManagerConfig {
    const ApiConfig& api;
    const LoggingConfig& logging;
    const TargetConfig& target;
    const TimingConfig& timing;
};

struct MarketDataThreadConfig {
    const StrategyConfig& strategy;
    const TimingConfig& timing;
    const TargetConfig& target;
};

struct AccountDataThreadConfig {
    const TimingConfig& timing;
};

struct ThreadConfigRegistry {
    ThreadSettings main;
    ThreadSettings trader_decision;
    ThreadSettings market_data;
    ThreadSettings account_data;
    ThreadSettings market_gate;
    ThreadSettings logging;
    
    ThreadConfigRegistry() = default;
};

} // namespace Config
} // namespace AlpacaTrader
