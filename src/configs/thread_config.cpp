#include "thread_config.hpp"
#include "system_config.hpp"

namespace AlpacaTrader {
namespace Config {

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
    throw std::runtime_error("Invalid priority string: " + str + ". Must be one of: REALTIME, HIGHEST, HIGH, NORMAL, LOW, LOWEST");
}

} // namespace Config
} // namespace AlpacaTrader
