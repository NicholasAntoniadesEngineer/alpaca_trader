// http_utils.hpp
#ifndef HTTP_UTILS_HPP
#define HTTP_UTILS_HPP

#include <string>
#include <vector>
#include "../data/data_structures.hpp"

// HTTP request wrapper to avoid multi-parameter functions
struct HttpRequest {
    std::string url;
    const std::string* api_key;
    const std::string* api_secret;
    const std::string* log_file;
    int retries;
    std::string body; // for POST; leave empty for GET

    HttpRequest(const std::string& u,
                const std::string& key,
                const std::string& secret,
                const std::string& logfile,
                int r = 3,
                std::string b = "")
        : url(u), api_key(&key), api_secret(&secret), log_file(&logfile), retries(r), body(std::move(b)) {}
};

size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* s);

std::string http_get(const HttpRequest& req);

std::string http_post(const HttpRequest& req);

std::string get_iso_time_minus(int minutes);



#endif // HTTP_UTILS_HPP
