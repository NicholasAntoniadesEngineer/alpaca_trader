// HttpUtils.cpp
#include "http_utils.hpp"
#include "core/utils/connectivity_manager.hpp"
#include "core/logging/logger/async_logger.hpp"
#include "time_utils.hpp"
#include <chrono>
#include <thread>
#include <ctime>
#include <curl/curl.h>
#include <cstdlib>
#include <string>

// Using declarations for cleaner code
using AlpacaTrader::Logging::log_message;

// Implement write_callback
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* s) {
    s->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// Implement http_get
std::string http_get(const HttpRequest& req) {
    static ConnectivityManager connectivity;
    auto& connectivity_ref = connectivity;
    
    // Check if we should attempt connection
    if (!connectivity_ref.should_attempt_connection()) {
        std::string status_msg = "Connectivity check failed - status: " + connectivity_ref.get_status_string() + 
                                ", retry in " + std::to_string(connectivity_ref.get_seconds_until_retry()) + "s";
        log_message(status_msg, *req.log_file);
        return "";
    }

    CURL* curl = curl_easy_init();
    std::string response;
    long http_response_code = 0;
    
    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("APCA-API-KEY-ID: " + *req.api_key).c_str());
        headers = curl_slist_append(headers, ("APCA-API-SECRET-KEY: " + *req.api_secret).c_str());
        curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, req.timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, req.enable_ssl_verification ? 1L : 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, req.enable_ssl_verification ? 2L : 0L);
        
        CURLcode res;
        bool success = false;
        
        for (int i = 0; i < req.retries; ++i) {
            res = curl_easy_perform(curl);
            
            // Get HTTP response code for better error reporting
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response_code);
            
            if (res == CURLE_OK) {
                success = true;
                break;
            }
            
            std::string error_msg = "HTTP GET retry " + std::to_string(i + 1) + "/" + std::to_string(req.retries) + 
                                  " failed: " + std::string(curl_easy_strerror(res)) + 
                                  " (HTTP " + std::to_string(http_response_code) + ")";
            log_message(error_msg, *req.log_file);
            connectivity_ref.report_failure(error_msg);
            
            if (i < req.retries - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(req.rate_limit_delay_ms));
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (success) {
            connectivity_ref.report_success();
            
            // Log successful response details for debugging
            if (response.empty()) {
                std::string empty_response_msg = "HTTP GET succeeded but returned empty response (HTTP " + 
                                               std::to_string(http_response_code) + ") for URL: " + req.url;
                log_message(empty_response_msg, *req.log_file);
            }
        } else {
            // Log final failure with detailed information
            std::string final_error = "HTTP GET failed after " + std::to_string(req.retries) + " retries. " +
                                    "Last error: " + std::string(curl_easy_strerror(res)) + 
                                    " (HTTP " + std::to_string(http_response_code) + ") " +
                                    "URL: " + req.url;
            log_message(final_error, *req.log_file);
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } else {
        log_message("Failed to initialize CURL for HTTP GET request", *req.log_file);
    }
    
    return response;
}

// Implement http_post
std::string http_post(const HttpRequest& req) {
    static ConnectivityManager connectivity;
    auto& connectivity_ref = connectivity;
    
    // Check if we should attempt connection
    if (!connectivity_ref.should_attempt_connection()) {
        std::string status_msg = "Connectivity check failed - status: " + connectivity_ref.get_status_string() + 
                                ", retry in " + std::to_string(connectivity_ref.get_seconds_until_retry()) + "s";
        log_message(status_msg, *req.log_file);
        return "";
    }

    CURL* curl = curl_easy_init();
    std::string response;
    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("APCA-API-KEY-ID: " + *req.api_key).c_str());
        headers = curl_slist_append(headers, ("APCA-API-SECRET-KEY: " + *req.api_secret).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, req.timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, req.enable_ssl_verification ? 1L : 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, req.enable_ssl_verification ? 2L : 0L);
        
        CURLcode res;
        bool success = false;
        
        for (int i = 0; i < req.retries; ++i) {
            res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                success = true;
                break;
            }
            
            std::string error_msg = "POST retry failed: " + std::string(curl_easy_strerror(res));
            log_message(error_msg, *req.log_file);
            connectivity_ref.report_failure(error_msg);
            
            if (i < req.retries - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(req.rate_limit_delay_ms));
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (success) {
            connectivity_ref.report_success();
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return response;
}

// Implement http_delete
std::string http_delete(const HttpRequest& req) {
    static ConnectivityManager connectivity;
    auto& connectivity_ref = connectivity;
    
    // Check if we should attempt connection
    if (!connectivity_ref.should_attempt_connection()) {
        std::string status_msg = "Connectivity check failed - status: " + connectivity_ref.get_status_string() + 
                                ", retry in " + std::to_string(connectivity_ref.get_seconds_until_retry()) + "s";
        log_message(status_msg, *req.log_file);
        return "";
    }

    CURL* curl = curl_easy_init();
    std::string response;
    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("APCA-API-KEY-ID: " + *req.api_key).c_str());
        headers = curl_slist_append(headers, ("APCA-API-SECRET-KEY: " + *req.api_secret).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, req.timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, req.enable_ssl_verification ? 1L : 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, req.enable_ssl_verification ? 2L : 0L);
        
        // Rate limiting
        if (req.rate_limit_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(req.rate_limit_delay_ms));
        }
        
        CURLcode res = curl_easy_perform(curl);
        bool success = (res == CURLE_OK);
        
        if (!success) {
            std::string error_msg = "HTTP DELETE failed: " + std::string(curl_easy_strerror(res));
            log_message(error_msg, *req.log_file);
            connectivity_ref.report_failure(error_msg);
        } else {
            connectivity_ref.report_success();
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return response;
}

// Implement get_iso_time_minus
std::string get_iso_time_minus(int minutes) {
    return TimeUtils::get_iso_time_minus_minutes(minutes);
}

// Implement replace_url_placeholder
std::string replace_url_placeholder(const std::string& url, const std::string& symbol) {
    std::string result = url;
    size_t pos = result.find("{symbol}");
    if (pos != std::string::npos) {
        result.replace(pos, 8, symbol);
    }
    return result;
}


