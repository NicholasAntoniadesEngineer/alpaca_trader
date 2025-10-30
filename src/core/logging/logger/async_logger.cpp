/**
 * Asynchronous logging system for high-performance trading operations.
 */
#include "async_logger.hpp"
#include "configs/system_config.hpp"
#include "core/trader/config_loader/config_loader.hpp"
#include "core/threads/thread_logic/platform/thread_control.hpp"
#include "core/utils/time_utils.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <ctime>
#include <cstdio>
#include <filesystem>

namespace AlpacaTrader {
namespace Logging {

static std::atomic<AsyncLogger*> g_async_logger{nullptr};
static thread_local std::string t_log_tag = "MAIN  ";
static std::atomic<LoggingContext*> g_logging_context{nullptr};

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
                auto* ctx = g_logging_context.load();
                if (!ctx) { throw std::runtime_error("Logging context not set"); }
                std::lock_guard<std::mutex> cguard(ctx->console_mutex);
                if (ctx->inline_active.load()) {
                    std::cout << std::endl;
                    ctx->inline_active.store(false);
                }
                std::cout << log_str << std::flush;
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
    auto* ctx = g_logging_context.load();
    if (!ctx) { return; }
    std::lock_guard<std::mutex> cguard(ctx->console_mutex);
    std::cout << "\r" << message << std::flush;
    ctx->inline_active.store(true);
}

void end_inline_status() {
    auto* ctx = g_logging_context.load();
    if (!ctx) { return; }
    std::lock_guard<std::mutex> cguard(ctx->console_mutex);
    if (ctx->inline_active.load()) {
        std::cout << std::endl;
        ctx->inline_active.store(false);
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

std::string create_unique_run_folder() {
    std::time_t now = std::time(nullptr);
    std::tm local_tm_buf;
    std::tm* local_tm = &local_tm_buf;
    localtime_r(&now, &local_tm_buf);
    std::string git_hash = get_git_commit_hash();

    // Create unique run folder: runtime_logs/run_DD-HH-MM_githash
    std::stringstream ss;
    ss << "runtime_logs/run_" << std::put_time(local_tm, TimeUtils::LOG_FILENAME) << "_" << git_hash;

    std::string run_folder = ss.str();

    // Create the directory structure
    try {
        std::filesystem::create_directories(run_folder);
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR: Failed to create run folder: " << e.what() << std::endl;
        throw std::runtime_error("Failed to create run folder: " + run_folder);
    }

    return run_folder;
}

std::string extract_base_filename(const std::string& full_path) {
    // Extract just the filename from a full path
    size_t last_slash = full_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        return full_path.substr(last_slash + 1);
    }
    return full_path;
}

std::string generate_timestamped_log_filename(const std::string& base_filename) {
    std::time_t now = std::time(nullptr);
    std::tm local_tm_buf;
    std::tm* local_tm = &local_tm_buf;
    localtime_r(&now, &local_tm_buf);
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
    // Create or fetch context
    auto* ctx = g_logging_context.load();
    if (!ctx) {
        throw std::runtime_error("Logging context not set before initialization");
    }

    // Create unique run folder for this instance
    ctx->run_folder = create_unique_run_folder();

    // Generate timestamped log filename in the run folder
    std::string base_filename = ctx->run_folder + "/" + extract_base_filename(config.logging.log_file);
    std::string timestamped_log_file = generate_timestamped_log_filename(base_filename);

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
    ctx->async_logger = logger;
    initialize_global_logger(*logger);
    set_log_thread_tag("MAIN  ");

    return logger;
}

std::shared_ptr<CSVBarsLogger> initialize_csv_bars_logger(const std::string& base_filename) {
    try {
        auto* ctx = g_logging_context.load();
        if (!ctx) {
            throw std::runtime_error("Logging context not available");
        }
        // Use the already created run folder
        if (ctx->run_folder.empty()) {
            throw std::runtime_error("Run folder not initialized - call initialize_application_foundation first");
        }

        std::string bars_filename = ctx->run_folder + "/" + extract_base_filename(base_filename) + "_bars";
        std::string timestamped_bars_filename = generate_timestamped_log_filename(bars_filename);
        auto bars_logger = std::make_shared<CSVBarsLogger>(timestamped_bars_filename);

        if (!bars_logger->is_initialized()) {
            throw std::runtime_error("Failed to initialize CSV bars logger");
        }

        ctx->csv_bars_logger = bars_logger;
        return bars_logger;
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR: Failed to initialize CSV bars logger: " << e.what() << std::endl;
        throw; // Re-throw to ensure system fails - no defaults allowed
    } catch (...) {
        std::cerr << "CRITICAL ERROR: Unknown error initializing CSV bars logger" << std::endl;
        throw std::runtime_error("Failed to initialize CSV bars logger");
    }
}

std::shared_ptr<CSVTradeLogger> initialize_csv_trade_logger(const std::string& base_filename) {
    try {
        auto* ctx = g_logging_context.load();
        if (!ctx) {
            throw std::runtime_error("Logging context not available");
        }
        // Use the already created run folder
        if (ctx->run_folder.empty()) {
            throw std::runtime_error("Run folder not initialized - call initialize_application_foundation first");
        }

        std::string trade_filename = ctx->run_folder + "/" + extract_base_filename(base_filename) + "_trades";
        std::string timestamped_trade_filename = generate_timestamped_log_filename(trade_filename);
        auto trade_logger = std::make_shared<CSVTradeLogger>(timestamped_trade_filename);

        if (!trade_logger->is_valid()) {
            throw std::runtime_error("Failed to initialize CSV trade logger");
        }

        ctx->csv_trade_logger = trade_logger;
        return trade_logger;
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR: Failed to initialize CSV trade logger: " << e.what() << std::endl;
        throw; // Re-throw to ensure system fails - no defaults allowed
    } catch (...) {
        std::cerr << "CRITICAL ERROR: Unknown error initializing CSV trade logger" << std::endl;
        throw std::runtime_error("Failed to initialize CSV trade logger");
    }
}

std::shared_ptr<CSVBarsLogger> get_csv_bars_logger() {
    auto* ctx = g_logging_context.load();
    return ctx ? ctx->csv_bars_logger : nullptr;
}

std::shared_ptr<CSVTradeLogger> get_csv_trade_logger() {
    auto* ctx = g_logging_context.load();
    return ctx ? ctx->csv_trade_logger : nullptr;
}

std::mutex& get_console_mutex() {
    auto* ctx = g_logging_context.load();
    if (!ctx) {
        static std::mutex fallback;
        return fallback;
    }
    return ctx->console_mutex;
}

std::atomic<bool>& get_inline_active_flag() {
    auto* ctx = g_logging_context.load();
    if (!ctx) {
        static std::atomic<bool> fallback{false};
        return fallback;
    }
    return ctx->inline_active;
}

const std::string& get_run_folder() {
    auto* ctx = g_logging_context.load();
    if (!ctx) {
        static std::string empty;
        return empty;
    }
    return ctx->run_folder;
}

void set_logging_context(LoggingContext& context) {
    g_logging_context.store(&context);
}

} // namespace Logging
} // namespace AlpacaTrader
