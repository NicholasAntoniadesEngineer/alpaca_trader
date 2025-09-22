// HttpUtils.cpp
#include "utils/http_utils.hpp"
#include "utils/connectivity_manager.hpp"
#include "../logging/async_logger.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <ctime>
#include <curl/curl.h>
#include <cmath>
#include <cstdlib>

// Implement write_callback
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* s) {
    s->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// Implement http_get
std::string http_get(const HttpRequest& req) {
    auto& connectivity = ConnectivityManager::instance();
    
    // Check if we should attempt connection
    if (!connectivity.should_attempt_connection()) {
        std::string status_msg = "Connectivity check failed - status: " + connectivity.get_status_string() + 
                                ", retry in " + std::to_string(connectivity.get_seconds_until_retry()) + "s";
        log_message(status_msg, *req.log_file);
        return "";
    }

    CURL* curl = curl_easy_init();
    std::string response;
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
            if (res == CURLE_OK) {
                success = true;
                break;
            }
            
            std::string error_msg = "HTTP retry failed: " + std::string(curl_easy_strerror(res));
            log_message(error_msg, *req.log_file);
            connectivity.report_failure(error_msg);
            
            if (i < req.retries - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(req.rate_limit_delay_ms));
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (success) {
            connectivity.report_success();
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return response;
}

// Implement http_post
std::string http_post(const HttpRequest& req) {
    auto& connectivity = ConnectivityManager::instance();
    
    // Check if we should attempt connection
    if (!connectivity.should_attempt_connection()) {
        std::string status_msg = "Connectivity check failed - status: " + connectivity.get_status_string() + 
                                ", retry in " + std::to_string(connectivity.get_seconds_until_retry()) + "s";
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
            connectivity.report_failure(error_msg);
            
            if (i < req.retries - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(req.rate_limit_delay_ms));
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (success) {
            connectivity.report_success();
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return response;
}

// Implement get_iso_time_minus
std::string get_iso_time_minus(int minutes) {
    auto now = std::chrono::system_clock::now() - std::chrono::minutes(minutes);
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}


