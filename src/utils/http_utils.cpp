// HttpUtils.cpp
#include "http_utils.hpp"
#include "utils/connectivity_manager.hpp"
#include "time_utils.hpp"
#include <chrono>
#include <thread>
#include <ctime>
#include <curl/curl.h>
#include <cstdlib>
#include <string>
#include <stdexcept>

// Implement write_callback
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response_string) {
    response_string->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// Implement http_get
std::string http_get(const HttpRequest& http_request, ConnectivityManager& connectivity_ref) {
    // Check if we should attempt connection
    if (!connectivity_ref.should_attempt_connection()) {
        std::string error_message = "Connectivity check failed - status: " + connectivity_ref.get_status_string() + 
                                   ", retry in " + std::to_string(connectivity_ref.get_seconds_until_retry()) + "s";
        throw std::runtime_error(error_message);
    }

    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) {
        throw std::runtime_error("Failed to initialize CURL for HTTP GET request");
    }

    std::string response;
    long http_response_code = 0;
    struct curl_slist* headers = nullptr;
    
    try {
        headers = curl_slist_append(headers, ("APCA-API-KEY-ID: " + *http_request.api_key).c_str());
        headers = curl_slist_append(headers, ("APCA-API-SECRET-KEY: " + *http_request.api_secret).c_str());
        curl_easy_setopt(curl_handle, CURLOPT_URL, http_request.url.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, http_request.timeout_seconds);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, http_request.enable_ssl_verification ? 1L : 0L);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, http_request.enable_ssl_verification ? 2L : 0L);
        
        CURLcode curl_result;
        bool success = false;
        
        for (int retry_attempt = 0; retry_attempt < http_request.retries; ++retry_attempt) {
            curl_result = curl_easy_perform(curl_handle);
            
            // Get HTTP response code for better error reporting
            curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_response_code);
            
            if (curl_result == CURLE_OK) {
                success = true;
                break;
            }
            
            std::string error_message = "HTTP GET retry " + std::to_string(retry_attempt + 1) + "/" + std::to_string(http_request.retries) + 
                                      " failed: " + std::string(curl_easy_strerror(curl_result)) + 
                                      " (HTTP " + std::to_string(http_response_code) + ")";
            connectivity_ref.report_failure(error_message);
            
            if (retry_attempt < http_request.retries - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(http_request.rate_limit_delay_ms));
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (success) {
            connectivity_ref.report_success();
            
            // Check for empty response even on successful HTTP request
            if (response.empty()) {
                std::string error_message = "HTTP GET succeeded but returned empty response (HTTP " + 
                                           std::to_string(http_response_code) + ") for URL: " + http_request.url;
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl_handle);
                throw std::runtime_error(error_message);
            }
            
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl_handle);
            return response;
        } else {
            // Final failure after all retries
            std::string error_message = "HTTP GET failed after " + std::to_string(http_request.retries) + " retries. " +
                                       "Last error: " + std::string(curl_easy_strerror(curl_result)) + 
                                       " (HTTP " + std::to_string(http_response_code) + ") " +
                                       "URL: " + http_request.url;
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl_handle);
            throw std::runtime_error(error_message);
        }
    } catch (...) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl_handle);
        throw;
    }
}

// Implement http_post
std::string http_post(const HttpRequest& http_request, ConnectivityManager& connectivity_ref) {
    // Check if we should attempt connection
    if (!connectivity_ref.should_attempt_connection()) {
        std::string error_message = "Connectivity check failed - status: " + connectivity_ref.get_status_string() + 
                                   ", retry in " + std::to_string(connectivity_ref.get_seconds_until_retry()) + "s";
        throw std::runtime_error(error_message);
    }

    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) {
        throw std::runtime_error("Failed to initialize CURL for HTTP POST request");
    }

    std::string response;
    struct curl_slist* headers = nullptr;
    
    try {
        headers = curl_slist_append(headers, ("APCA-API-KEY-ID: " + *http_request.api_key).c_str());
        headers = curl_slist_append(headers, ("APCA-API-SECRET-KEY: " + *http_request.api_secret).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl_handle, CURLOPT_URL, http_request.url.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, http_request.body.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, http_request.timeout_seconds);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, http_request.enable_ssl_verification ? 1L : 0L);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, http_request.enable_ssl_verification ? 2L : 0L);
        
        CURLcode curl_result;
        bool success = false;
        
        for (int retry_attempt = 0; retry_attempt < http_request.retries; ++retry_attempt) {
            curl_result = curl_easy_perform(curl_handle);
            if (curl_result == CURLE_OK) {
                success = true;
                break;
            }
            
            std::string error_message = "HTTP POST retry " + std::to_string(retry_attempt + 1) + "/" + std::to_string(http_request.retries) + 
                                      " failed: " + std::string(curl_easy_strerror(curl_result));
            connectivity_ref.report_failure(error_message);
            
            if (retry_attempt < http_request.retries - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(http_request.rate_limit_delay_ms));
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (success) {
            connectivity_ref.report_success();
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl_handle);
            return response;
        } else {
            std::string error_message = "HTTP POST failed after " + std::to_string(http_request.retries) + " retries. " +
                                       "Last error: " + std::string(curl_easy_strerror(curl_result)) + 
                                       " URL: " + http_request.url;
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl_handle);
            throw std::runtime_error(error_message);
        }
    } catch (...) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl_handle);
        throw;
    }
}

// Implement http_delete
std::string http_delete(const HttpRequest& http_request, ConnectivityManager& connectivity_ref) {
    // Check if we should attempt connection
    if (!connectivity_ref.should_attempt_connection()) {
        std::string error_message = "Connectivity check failed - status: " + connectivity_ref.get_status_string() + 
                                   ", retry in " + std::to_string(connectivity_ref.get_seconds_until_retry()) + "s";
        throw std::runtime_error(error_message);
    }

    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) {
        throw std::runtime_error("Failed to initialize CURL for HTTP DELETE request");
    }

    std::string response;
    struct curl_slist* headers = nullptr;
    
    try {
        headers = curl_slist_append(headers, ("APCA-API-KEY-ID: " + *http_request.api_key).c_str());
        headers = curl_slist_append(headers, ("APCA-API-SECRET-KEY: " + *http_request.api_secret).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl_handle, CURLOPT_URL, http_request.url.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, http_request.timeout_seconds);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, http_request.enable_ssl_verification ? 1L : 0L);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, http_request.enable_ssl_verification ? 2L : 0L);
        
        // Rate limiting
        if (http_request.rate_limit_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(http_request.rate_limit_delay_ms));
        }
        
        CURLcode curl_result = curl_easy_perform(curl_handle);
        bool success = (curl_result == CURLE_OK);
        
        if (!success) {
            std::string error_message = "HTTP DELETE failed: " + std::string(curl_easy_strerror(curl_result)) + 
                                       " URL: " + http_request.url;
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl_handle);
            connectivity_ref.report_failure(error_message);
            throw std::runtime_error(error_message);
        } else {
            connectivity_ref.report_success();
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl_handle);
            return response;
        }
    } catch (...) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl_handle);
        throw;
    }
}

// Implement get_iso_time_minus
std::string get_iso_time_minus(int minutes) {
    return TimeUtils::get_iso_time_minus_minutes(minutes);
}

// Implement replace_url_placeholder
std::string replace_url_placeholder(const std::string& request_url, const std::string& symbol) {
    std::string result_url = request_url;
    size_t placeholder_position = result_url.find("{symbol}");
    if (placeholder_position != std::string::npos) {
        result_url.replace(placeholder_position, 8, symbol);
    }
    return result_url;
}


