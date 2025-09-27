#ifndef ASYNC_LOGGER_HPP
#define ASYNC_LOGGER_HPP

#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <thread>
#include <memory>

// Forward declaration
struct SystemConfig;

namespace AlpacaTrader {
namespace Logging {

// Named constants
constexpr int LOG_TAG_WIDTH = 6;
static_assert(LOG_TAG_WIDTH > 0, "LOG_TAG_WIDTH must be positive");

class AsyncLogger {
private:
    std::string file_path;

public:
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<std::string> queue;
    std::atomic<bool> running{false};

    explicit AsyncLogger(const std::string& log_file_path) : file_path(log_file_path) {}
    
    const std::string& get_file_path() const { return file_path; }
    void enqueue(const std::string& formatted_line);
    void stop();
};

void set_async_logger(AsyncLogger* logger);

// Global console synchronization
extern std::mutex g_console_mtx;
extern std::atomic<bool> g_inline_active;

// Console inline status (no newline, overwrites same line; not written to file)
void log_inline_status(const std::string& message);
void end_inline_status();

// Formats inline messages with timestamp and thread tag
std::string get_formatted_inline_message(const std::string& content);

// Thread-local log tag (6 characters, padded/truncated) to appear in timestamp
void set_log_thread_tag(const std::string& tag6);

// Main logging function
void log_message(const std::string& message, const std::string& log_file_path);

// Utility functions for log file naming
std::string get_git_commit_hash();
std::string generate_timestamped_log_filename(const std::string& base_filename);

// Global lifecycle helpers
void initialize_global_logger(AsyncLogger& logger);
void shutdown_global_logger(AsyncLogger& logger);

// Application foundation initialization
std::shared_ptr<AsyncLogger> initialize_application_foundation(const SystemConfig& config);

} // namespace Logging
} // namespace AlpacaTrader

#endif // ASYNC_LOGGER_HPP
