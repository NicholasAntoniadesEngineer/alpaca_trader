#include "thread_config.hpp"

namespace ThreadSystem {

ThreadConfig ConfigProvider::get_default_config(Type type) {
    switch (type) {
        case Type::MAIN:
            return ThreadConfig(Priority::NORMAL, -1, "MAIN");
            
        case Type::TRADER_DECISION:
            return ThreadConfig(Priority::HIGHEST, -1, "TRADER");  // Critical trading decisions
            
        case Type::MARKET_DATA:
            return ThreadConfig(Priority::HIGH, -1, "MARKET");     // Time-sensitive market data
            
        case Type::ACCOUNT_DATA:
            return ThreadConfig(Priority::NORMAL, -1, "ACCOUNT");  // Standard account polling
            
        case Type::MARKET_GATE:
            return ThreadConfig(Priority::LOW, -1, "GATE");        // Market timing control
            
        case Type::LOGGING:
            return ThreadConfig(Priority::LOWEST, -1, "LOGGER");   // Background logging
            
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

} // namespace ThreadSystem
