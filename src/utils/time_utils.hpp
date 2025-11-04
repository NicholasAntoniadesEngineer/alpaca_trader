#ifndef TIME_UTILS_HPP
#define TIME_UTILS_HPP

#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace TimeUtils {

// Time format constants
constexpr const char* ISO_8601_WITH_Z = "%Y-%m-%dT%H:%M:%SZ";
constexpr const char* ISO_8601_WITHOUT_Z = "%Y-%m-%dT%H:%M:%S";
constexpr const char* HUMAN_READABLE = "%Y-%m-%d %H:%M:%S";
constexpr const char* LOG_FILENAME = "%d-%H-%M";

// Common time utility functions
std::string get_current_iso_time_with_z();
std::string get_current_iso_time_without_z();
std::string get_current_human_readable_time();
std::string get_iso_time_minus_minutes(int minutes);
std::string get_iso_time_plus_minutes(int minutes);

// Time parsing functions
std::tm parse_iso_time(const std::string& timestamp);
std::tm parse_iso_time_with_z(const std::string& timestamp);

// Timestamp conversion functions
std::string convert_milliseconds_to_human_readable(const std::string& milliseconds_timestamp);

} // namespace TimeUtils

#endif // TIME_UTILS_HPP
