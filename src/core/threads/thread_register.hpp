#pragma once

#include "core/threads/thread_logic/thread_types.hpp"
#include "configs/strategy_config.hpp"
#include "configs/timing_config.hpp"
#include "configs/api_config.hpp"
#include "configs/logging_config.hpp"
#include "configs/api_client_config.hpp"
#include <map>
#include <string>

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

struct AccountManagerConfig {
    const ApiConfig& api;
    const LoggingConfig& logging;
    const TimingConfig& timing;
    const StrategyConfig& strategy;
};

struct MarketDataThreadConfig {
    const StrategyConfig& strategy;
    const TimingConfig& timing;
};

struct AccountDataThreadConfig {
    const TimingConfig& timing;
};

struct ThreadConfigRegistry {
    std::map<std::string, ThreadSettings> thread_settings;

    ThreadConfigRegistry() = default;
    
    // Generic thread settings access - only returns settings if explicitly configured
    const ThreadSettings& get_thread_settings(const std::string& thread_name) const {
        auto it = thread_settings.find(thread_name);
        if (it != thread_settings.end()) {
            return it->second;
        }
        // Thread settings not found - this should never happen in a properly configured system
        throw std::runtime_error("Thread settings not found for thread: " + thread_name + 
                                ". Ensure thread configuration is loaded from CSV.");
    }
    
    bool has_thread_settings(const std::string& thread_name) const {
        return thread_settings.find(thread_name) != thread_settings.end();
    }
    
    // Non-const version for config loading only
    ThreadSettings& get_thread_settings_for_loading(const std::string& thread_name) {
        return thread_settings[thread_name];
    }
};

} // namespace Config
} // namespace AlpacaTrader
