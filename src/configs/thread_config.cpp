#include "thread_config.hpp"
#include "system_config.hpp"

namespace AlpacaTrader {
namespace Config {


ThreadConfig ConfigProvider::get_config_from_system(Type type, const SystemConfig& system_config) {
    switch (type) {
        case Type::MAIN:
            return system_config.thread.main;
            
        case Type::TRADER_DECISION:
            return system_config.thread.trader_decision;
            
        case Type::MARKET_DATA:
            return system_config.thread.market_data;
            
        case Type::ACCOUNT_DATA:
            return system_config.thread.account_data;
            
        case Type::MARKET_GATE:
            return system_config.thread.market_gate;
            
        case Type::LOGGING:
            return system_config.thread.logging;
            
        default:
            return ThreadConfig(Priority::NORMAL, -1, "UNKNOWN");
    }
}

std::string ConfigProvider::priority_to_string(Priority priority) {
    switch (priority) {
        case Priority::REALTIME: return "REALTIME";
        case Priority::HIGHEST:  return "HIGHEST";
        case Priority::HIGH:     return "HIGH";
        case Priority::NORMAL:   return "NORMAL";
        case Priority::LOW:      return "LOW";
        case Priority::LOWEST:   return "LOWEST";
        default:                 return "UNKNOWN";
    }
}

Priority ConfigProvider::string_to_priority(const std::string& str) {
    if (str == "REALTIME") return Priority::REALTIME;
    if (str == "HIGHEST")  return Priority::HIGHEST;
    if (str == "HIGH")     return Priority::HIGH;
    if (str == "NORMAL")   return Priority::NORMAL;
    if (str == "LOW")      return Priority::LOW;
    if (str == "LOWEST")   return Priority::LOWEST;
    return Priority::NORMAL; // Default fallback
}

} // namespace Config
} // namespace AlpacaTrader
