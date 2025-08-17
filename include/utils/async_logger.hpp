#ifndef ASYNC_LOGGER_HPP
#define ASYNC_LOGGER_HPP

#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <thread>

// Named constants
constexpr int LOG_TAG_WIDTH = 6;

class AsyncLogger {
private:
    std::string file_path;
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<std::string> queue;
    std::atomic<bool> running{false};
    std::thread worker;

    void run();

public:
    explicit AsyncLogger(const std::string& log_file_path) : file_path(log_file_path) {}
    void start();
    void stop();
    void enqueue(const std::string& formatted_line);
};

void set_async_logger(AsyncLogger* logger);

// Console inline status (no newline, overwrites same line; not written to file)
void log_inline_status(const std::string& message);
void end_inline_status();

// Thread-local log tag (6 characters, padded/truncated) to appear in timestamp
void set_log_thread_tag(const std::string& tag6);

// Main logging function
void log_message(const std::string& message, const std::string& log_file_path);

// Global lifecycle helpers
void initialize_global_logger(AsyncLogger& logger);
void shutdown_global_logger(AsyncLogger& logger);

#endif // ASYNC_LOGGER_HPP
