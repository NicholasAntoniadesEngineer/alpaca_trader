#ifndef HTTP_UTILS_HPP
#define HTTP_UTILS_HPP

#include <string>
#include <vector>
#include "core/trader/data_structures/data_structures.hpp"
#include "core/utils/connectivity_manager.hpp"

// HTTP request wrapper to avoid multi-parameter functions
struct HttpRequest {
    std::string url;
    const std::string* api_key;
    const std::string* api_secret;
    const std::string* log_file;
    int retries;
    int timeout_seconds;
    bool enable_ssl_verification;
    int rate_limit_delay_ms;
    std::string body; // for POST; leave empty for GET

    HttpRequest(const std::string& request_url,
                const std::string& api_key_string,
                const std::string& api_secret_string,
                const std::string& log_file_path,
                int retry_count,
                int timeout_seconds_param,
                bool enable_ssl_verification_param,
                int rate_limit_delay_milliseconds,
                const std::string& request_body)
        : url(request_url), api_key(&api_key_string), api_secret(&api_secret_string), log_file(&log_file_path), 
          retries(retry_count), timeout_seconds(timeout_seconds_param), enable_ssl_verification(enable_ssl_verification_param),
          rate_limit_delay_ms(rate_limit_delay_milliseconds), body(request_body) {}
};

size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response_string);

std::string http_get(const HttpRequest& http_request, ConnectivityManager& connectivity_manager);

std::string http_post(const HttpRequest& http_request, ConnectivityManager& connectivity_manager);

std::string http_delete(const HttpRequest& http_request, ConnectivityManager& connectivity_manager);

std::string get_iso_time_minus(int minutes);

std::string replace_url_placeholder(const std::string& request_url, const std::string& symbol);



#endif // HTTP_UTILS_HPP
