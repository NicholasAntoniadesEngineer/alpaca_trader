#include "time_utils.hpp"
#include <ctime>

namespace TimeUtils {

std::string get_current_iso_time_with_z() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    
    // Use thread-safe gmtime_r instead of gmtime
    struct tm timeinfo;
    gmtime_r(&in_time_t, &timeinfo);
    ss << std::put_time(&timeinfo, ISO_8601_WITH_Z);
    return ss.str();
}

std::string get_current_iso_time_without_z() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    
    // Use thread-safe gmtime_r instead of gmtime
    struct tm timeinfo;
    gmtime_r(&in_time_t, &timeinfo);
    ss << std::put_time(&timeinfo, ISO_8601_WITHOUT_Z);
    return ss.str();
}

std::string get_current_human_readable_time() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    
    // Use thread-safe localtime_r instead of localtime
    struct tm timeinfo;
    localtime_r(&in_time_t, &timeinfo);
    ss << std::put_time(&timeinfo, HUMAN_READABLE);
    return ss.str();
}

std::string get_iso_time_minus_minutes(int minutes) {
    auto now = std::chrono::system_clock::now() - std::chrono::minutes(minutes);
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    
    // Use thread-safe gmtime_r instead of gmtime
    struct tm timeinfo;
    gmtime_r(&in_time_t, &timeinfo);
    ss << std::put_time(&timeinfo, ISO_8601_WITH_Z);
    return ss.str();
}

std::string get_iso_time_plus_minutes(int minutes) {
    auto now = std::chrono::system_clock::now() + std::chrono::minutes(minutes);
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    
    // Use thread-safe gmtime_r instead of gmtime
    struct tm timeinfo;
    gmtime_r(&in_time_t, &timeinfo);
    ss << std::put_time(&timeinfo, ISO_8601_WITH_Z);
    return ss.str();
}

std::tm parse_iso_time(const std::string& timestamp) {
    std::tm t = {};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&t, ISO_8601_WITHOUT_Z);
    return t;
}

std::tm parse_iso_time_with_z(const std::string& timestamp) {
    std::tm t = {};
    std::string base_timestamp = timestamp;
    
    // Handle Z suffix
    if (base_timestamp.back() == 'Z') {
        base_timestamp.pop_back();
    }
    
    // Handle timezone offset (e.g., +05:00, -08:00)
    size_t tz_pos = base_timestamp.find_first_of("+-", 19);
    if (tz_pos != std::string::npos) {
        base_timestamp = base_timestamp.substr(0, tz_pos);
    }
    
    std::istringstream ss(base_timestamp);
    ss >> std::get_time(&t, ISO_8601_WITHOUT_Z);
    return t;
}

std::string convert_milliseconds_to_human_readable(const std::string& milliseconds_timestamp) {
    try {
        long long timestamp_millis = std::stoll(milliseconds_timestamp);
        time_t timestamp_seconds = timestamp_millis / MILLISECONDS_PER_SECOND;
        
        struct tm timeinfo;
        localtime_r(&timestamp_seconds, &timeinfo);
        
        std::stringstream ss;
        ss << std::put_time(&timeinfo, HUMAN_READABLE);
        return ss.str();
    } catch (const std::exception&) {
        // If conversion fails, return original timestamp
        return milliseconds_timestamp;
    }
}

} // namespace TimeUtils
