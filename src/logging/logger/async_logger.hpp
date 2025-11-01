#ifndef ASYNC_LOGGER_HPP
#define ASYNC_LOGGER_HPP

#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <thread>
#include <memory>
#include <unordered_map>
#include "configs/system_config.hpp"
#include "csv_bars_logger.hpp"
#include "csv_trade_logger.hpp"

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


struct LoggingContext {
    std::shared_ptr<AsyncLogger> async_logger;
    std::shared_ptr<CSVBarsLogger> csv_bars_logger;
    std::shared_ptr<CSVTradeLogger> csv_trade_logger;
    std::mutex console_mutex;
    std::atomic<bool> inline_active{false};
    std::string run_folder;
    mutable std::mutex thread_tag_mutex;
    std::unordered_map<std::thread::id, std::string> thread_tags;
    
    std::string get_thread_tag() const {
        std::lock_guard<std::mutex> thread_tag_lock(thread_tag_mutex);
        std::unordered_map<std::thread::id, std::string>::const_iterator thread_tag_map_iterator = thread_tags.find(std::this_thread::get_id());
        if (thread_tag_map_iterator != thread_tags.end()) {
            return thread_tag_map_iterator->second;
        }
        return "MAIN  ";
    }
    
    void set_thread_tag(const std::string& tag_value) {
        std::lock_guard<std::mutex> lock(thread_tag_mutex);
        std::string tag_string = tag_value;
        if (tag_string.size() < LOG_TAG_WIDTH) {
            tag_string.append(LOG_TAG_WIDTH - tag_string.size(), ' ');
        }
        if (tag_string.size() > LOG_TAG_WIDTH) {
            tag_string = tag_string.substr(0, LOG_TAG_WIDTH);
        }
        thread_tags[std::this_thread::get_id()] = tag_string;
    }
};

// Console inline status (no newline, overwrites same line; not written to file)
void log_inline_status(const std::string& message);
void end_inline_status();

// Formats inline messages with timestamp and thread tag
std::string get_formatted_inline_message(const std::string& content);

// Thread-local log tag (6 characters, padded/truncated) to appear in timestamp
void set_log_thread_tag(const std::string& thread_tag_value);

// Main logging function
void log_message(const std::string& message, const std::string& log_file_path);

// Utility functions for log file naming
std::string get_git_commit_hash();
std::string generate_timestamped_log_filename(const std::string& base_filename);

// Global lifecycle helpers (use context internally)
void initialize_global_logger(AsyncLogger& logger);
void shutdown_global_logger(AsyncLogger& logger);

// Application foundation initialization
std::shared_ptr<AsyncLogger> initialize_application_foundation(const AlpacaTrader::Config::SystemConfig& config);

// CSV logging initialization
std::shared_ptr<CSVBarsLogger> initialize_csv_bars_logger(const std::string& base_filename);
std::shared_ptr<CSVTradeLogger> initialize_csv_trade_logger(const std::string& base_filename);

// Context-backed accessors (no externs)
std::shared_ptr<CSVBarsLogger> get_csv_bars_logger();
std::shared_ptr<CSVTradeLogger> get_csv_trade_logger();
std::mutex& get_console_mutex();
std::atomic<bool>& get_inline_active_flag();
const std::string& get_run_folder();
void set_logging_context(LoggingContext& context);

} // namespace Logging
} // namespace AlpacaTrader

#endif // ASYNC_LOGGER_HPP
