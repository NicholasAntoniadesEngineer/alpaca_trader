#ifndef HTTP_UTILS_HPP
#define HTTP_UTILS_HPP

#include <string>
#include <vector>
#include "core/data_structures.hpp"
#include "connectivity_manager.hpp"

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

    HttpRequest(const std::string& u,
                const std::string& key,
                const std::string& secret,
                const std::string& logfile,
                int r = 3,
                int timeout = 30,
                bool ssl_verify = true,
                int rate_delay = 100,
                std::string b = "")
        : url(u), api_key(&key), api_secret(&secret), log_file(&logfile), 
          retries(r), timeout_seconds(timeout), enable_ssl_verification(ssl_verify),
          rate_limit_delay_ms(rate_delay), body(std::move(b)) {}
};

size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* s);

std::string http_get(const HttpRequest& req);

std::string http_post(const HttpRequest& req);

std::string http_delete(const HttpRequest& req);

std::string get_iso_time_minus(int minutes);



#endif // HTTP_UTILS_HPP
