#include "market_clock.hpp"
#include "../../logging/async_logger.hpp"
#include "../../utils/http_utils.hpp"
#include "../../json/json.hpp"
#include <iomanip>
#include <sstream>
#include <cmath>

using json = nlohmann::json;

bool MarketClock::is_core_trading_hours() const {
    HttpRequest req(api.base_url + "/v2/clock", api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
    std::string response = http_get(req);
    if (response.empty()) return false;
    
    try {
        json j = json::parse(response);
        if (!j.contains("is_open") || j["is_open"].is_null()) return false;
        if (!j["is_open"]) return false;
        if (!j.contains("timestamp") || j["timestamp"].is_null()) return false;
        
        std::string timestamp = j["timestamp"];
        std::tm t = parse_timestamp(timestamp);
        
        // Check if timestamp already includes timezone info
        bool has_timezone = timestamp.find_first_of("+-Z") != std::string::npos;
        int hour, min;
        
        if (has_timezone) {
            // Timestamp already includes timezone, use as-is
            hour = t.tm_hour;
            min = t.tm_min;
        } else {
            // Assume UTC and convert to ET
            hour = t.tm_hour + session.et_utc_offset_hours;
            min = t.tm_min;
        }
        
        return is_within_time_window(hour, min, session.market_open_hour, session.market_open_minute, 
                                   session.market_close_hour, session.market_close_minute);
    } catch (...) {
        log_message("Error parsing clock response: " + response, logging.log_file);
    }
    return false;
}

bool MarketClock::is_within_fetch_window() const {
    // Allow data fetching only when: market is open OR within N minutes before next open
    HttpRequest req(api.base_url + "/v2/clock", api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
    std::string response = http_get(req);
    if (response.empty()) return false;
    
    try {
        json j = json::parse(response);
        if (j.contains("is_open") && j["is_open"].is_boolean() && j["is_open"].get<bool>()) {
            return true;
        }
        
        if (j.contains("timestamp") && !j["timestamp"].is_null() && 
            j.contains("next_open") && !j["next_open"].is_null()) {
            
            std::string now_s = j["timestamp"].get<std::string>();
            std::string next_open_s = j["next_open"].get<std::string>();
            std::tm now_tm = parse_timestamp(now_s);
            std::tm open_tm = parse_timestamp(next_open_s);
            
            // Convert to minutes since epoch (approx; ignore TZ suffix fractional seconds)
            long long now_min = static_cast<long long>(std::mktime(const_cast<std::tm*>(&now_tm)) / 60);
            long long open_min = static_cast<long long>(std::mktime(const_cast<std::tm*>(&open_tm)) / 60);
            
            if (now_min > 0 && open_min > 0) {
                long long mins_to_open = open_min - now_min;
                return mins_to_open >= 0 && mins_to_open <= timing.pre_open_buffer_min;
            }
        }
    } catch (...) {
        log_message("Error parsing clock response for fetch window: " + response, logging.log_file);
    }
    return false;
}

std::tm MarketClock::parse_timestamp(const std::string& timestamp) const {
    std::tm t = {};
    
    // Find the position of the fractional seconds (if any)
    size_t dot_pos = timestamp.find('.');
    size_t tz_pos = timestamp.find_first_of("+-Z", dot_pos);
    
    std::string base_timestamp = timestamp;
    if (dot_pos != std::string::npos) {
        // Remove fractional seconds
        base_timestamp = timestamp.substr(0, dot_pos);
        if (tz_pos != std::string::npos) {
            base_timestamp += timestamp.substr(tz_pos);
        }
    }
    
    // Parse the base timestamp (YYYY-MM-DDTHH:MM:SS or YYYY-MM-DDTHH:MM:SSZ)
    std::istringstream ss(base_timestamp);
    if (base_timestamp.back() == 'Z') {
        // Remove Z and parse as UTC
        base_timestamp.pop_back();
        ss.str(base_timestamp);
        ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S");
    } else if (tz_pos != std::string::npos && timestamp[tz_pos] != 'Z') {
        // Parse without timezone info for now
        std::string no_tz = base_timestamp.substr(0, tz_pos);
        ss.str(no_tz);
        ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S");
    } else {
        ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S");
    }
    
    return t;
}

bool MarketClock::is_within_time_window(int hour, int minute, int open_hour, int open_minute, 
                                       int close_hour, int close_minute) const {
    bool after_open = (hour > open_hour) || (hour == open_hour && minute >= open_minute);
    bool before_close = (hour < close_hour) || (hour == close_hour && minute <= close_minute);
    return after_open && before_close;
}
