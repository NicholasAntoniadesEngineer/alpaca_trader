#pragma once

#include "thread_config.hpp"

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
