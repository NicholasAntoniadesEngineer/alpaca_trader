/**
 * Asynchronous logging system for high-performance trading operations.
 */
#include "logging/async_logger.hpp"
#include "configs/system_config.hpp"
#include "configs/config_loader.hpp"
#include "threads/thread_logic/platform/thread_control.hpp"
#include "utils/time_utils.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <ctime>
#include <cstdio>

namespace AlpacaTrader {
namespace Logging {

static std::atomic<AsyncLogger*> g_async_logger{nullptr};
static thread_local std::string t_log_tag = "MAIN  ";
std::mutex g_console_mtx;
std::atomic<bool> g_inline_active{false};

void set_async_logger(AsyncLogger* logger) {
    g_async_logger.store(logger);
}

void set_log_thread_tag(const std::string& tag6) {
    std::string t = tag6;
    if (t.size() < LOG_TAG_WIDTH) t.append(LOG_TAG_WIDTH - t.size(), ' ');
    if (t.size() > LOG_TAG_WIDTH) t = t.substr(0, LOG_TAG_WIDTH);
    t_log_tag = t;
}


void log_message(const std::string& message, const std::string& log_file_path) {
    try {
        // Basic fallback if time utils fails
        std::string timestamp;
        try {
            timestamp = TimeUtils::get_current_human_readable_time();
        } catch (...) {
            timestamp = "ERROR-TIME";
        }
        
        std::stringstream ss;
        ss << timestamp << " [" << t_log_tag << "]   " << message << std::endl;
        std::string log_str = ss.str();

        AsyncLogger* logger = g_async_logger.load();
        if (logger) {
            try {
                logger->enqueue(log_str);
                return;
            } catch (...) {
                // Fall through to console logging if async logger fails
            }
        }

        {
            try {
                std::lock_guard<std::mutex> cguard(g_console_mtx);
                if (g_inline_active.load()) {
                    std::cout << std::endl;
                    g_inline_active.store(false);
                }
                std::cout << log_str;
            } catch (...) {
                // Fall back to basic console output
                std::cout << message << std::endl;
            }
        }
        
        // Only write to file if log_file_path is not empty
        if (!log_file_path.empty()) {
            try {
                std::ofstream log_file(log_file_path, std::ios::app);
                if (log_file.is_open()) {
                    log_file << log_str;
                    log_file.close();
                }
            } catch (...) {
                // Ignore file logging errors
            }
        }
    } catch (...) {
        // Ultimate fallback - just output the message
        std::cout << message << std::endl;
    }
}


void log_inline_status(const std::string& message) {
    std::lock_guard<std::mutex> cguard(g_console_mtx);
    std::cout << "\r" << message << std::flush;
    g_inline_active.store(true);
}

void end_inline_status() {
    std::lock_guard<std::mutex> cguard(g_console_mtx);
    if (g_inline_active.load()) {
        std::cout << std::endl;
        g_inline_active.store(false);
    }
}

std::string get_formatted_inline_message(const std::string& content) {
    std::string timestamp = TimeUtils::get_current_human_readable_time();
    std::stringstream ss;
    ss << timestamp << " [" << t_log_tag << "]   " << content;
    return ss.str();
}

std::string get_git_commit_hash() {
    // Try to get git hash using the same command as Makefile
    FILE* pipe = popen("git rev-parse --short HEAD 2>/dev/null", "r");
    if (!pipe) {
        return "unknown";
    }
    
    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    
    // Remove trailing newline if present
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    return result.empty() ? "unknown" : result;
}

std::string generate_timestamped_log_filename(const std::string& base_filename) {
    std::time_t now = std::time(nullptr);
    std::tm* local_tm = std::localtime(&now);
    std::string git_hash = get_git_commit_hash();
    
    // Extract base name without extension
    std::string base_name = base_filename;
    std::string extension = "";
    
    size_t dot_pos = base_filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        base_name = base_filename.substr(0, dot_pos);
        extension = base_filename.substr(dot_pos);
    }
    
    // Create timestamped filename with git hash: base_name_DD-HH-MM_githash.extension
    std::stringstream ss;
    ss << base_name << "_" << std::put_time(local_tm, TimeUtils::LOG_FILENAME) << "_" << git_hash << extension;
    return ss.str();
}

void initialize_global_logger(AsyncLogger& logger) {
    set_async_logger(&logger);
    // Thread will be started separately via SystemThreads
}

void shutdown_global_logger(AsyncLogger& logger) {
    logger.stop();
}

// AsyncLogger implementation
void AsyncLogger::stop() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        running.store(false);
    }
    cv.notify_all();
}

void AsyncLogger::enqueue(const std::string& formatted_line) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(formatted_line);
    }
    cv.notify_one();
}

std::shared_ptr<AsyncLogger> initialize_application_foundation(const AlpacaTrader::Config::SystemConfig& config) {
    // Generate timestamped log filename
    std::string timestamped_log_file = generate_timestamped_log_filename(config.logging.log_file);
    
    // Create logger instance
    auto logger = std::make_shared<AsyncLogger>(timestamped_log_file);
    
    // Validate configuration
    std::string cfg_error;
    if (!validate_config(config, cfg_error)) {
        // Log error using logging system
        log_message("ERROR: Config error: " + cfg_error, "");
        exit(1);
    }
    
    // Initialize global logger (logger already created with timestamped filename)
    initialize_global_logger(*logger);
    set_log_thread_tag("MAIN  ");
    
    return logger;
}

} // namespace Logging
} // namespace AlpacaTrader
