#include "async_logger.hpp"
#include "configs/system_config.hpp"
#include "trader/config_loader/config_loader.hpp"
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
#include <filesystem>

namespace AlpacaTrader {
namespace Logging {

thread_local LoggingContext* thread_local_logging_context_pointer = nullptr;

void log_message_to_stderr(const std::string& error_message);

LoggingContext* get_logging_context() {
    LoggingContext* thread_logging_context_ptr = thread_local_logging_context_pointer;
    if (!thread_logging_context_ptr) {
        throw std::runtime_error("Logging context not initialized for current thread - system must fail without context");
    }
    return thread_logging_context_ptr;
}

void set_log_thread_tag(const std::string& thread_tag_value) {
    LoggingContext* thread_logging_context_ptr = get_logging_context();
    thread_logging_context_ptr->set_thread_tag(thread_tag_value);
}


void log_message(const std::string& message, const std::string& log_file_path) {
    try {
        LoggingContext* thread_logging_context_ptr = get_logging_context();
        
        std::string timestamp_string;
        try {
            timestamp_string = TimeUtils::get_current_human_readable_time();
        } catch (const std::exception& time_exception_error) {
            log_message_to_stderr("ERROR: TimeUtils failed: " + std::string(time_exception_error.what()));
            timestamp_string = "ERROR-TIME";
        } catch (...) {
            log_message_to_stderr("ERROR: Unknown time utility failure");
            timestamp_string = "ERROR-TIME";
        }
        
        std::string thread_tag_string = thread_logging_context_ptr->get_thread_tag();
        std::stringstream log_stream;
        log_stream << timestamp_string << " [" << thread_tag_string << "]   " << message << std::endl;
        std::string log_formatted_string = log_stream.str();

        if (thread_logging_context_ptr->async_logger) {
            try {
                thread_logging_context_ptr->async_logger->enqueue(log_formatted_string);
                return;
            } catch (const std::exception& logger_exception_error) {
                log_message_to_stderr("ERROR: Async logger enqueue failed: " + std::string(logger_exception_error.what()));
            } catch (...) {
                log_message_to_stderr("ERROR: Unknown async logger failure");
            }
        }

        try {
            std::lock_guard<std::mutex> console_guard(thread_logging_context_ptr->console_mutex);
            if (thread_logging_context_ptr->inline_active.load()) {
                std::cout << std::endl;
                thread_logging_context_ptr->inline_active.store(false);
            }
            std::cout << log_formatted_string << std::flush;
        } catch (const std::exception& console_exception_error) {
            log_message_to_stderr("ERROR: Console logging failed: " + std::string(console_exception_error.what()));
        } catch (...) {
            log_message_to_stderr("ERROR: Unknown console logging failure");
        }
        
        if (!log_file_path.empty()) {
            try {
                std::ofstream log_file_stream(log_file_path, std::ios::app);
                if (log_file_stream.is_open()) {
                    log_file_stream << log_formatted_string;
                    log_file_stream.close();
                } else {
                    log_message_to_stderr("ERROR: Failed to open log file: " + log_file_path);
                }
            } catch (const std::exception& file_exception_error) {
                log_message_to_stderr("ERROR: File logging failed: " + std::string(file_exception_error.what()));
            } catch (...) {
                log_message_to_stderr("ERROR: Unknown file logging failure");
            }
        }
    } catch (const std::exception& critical_exception_error) {
        log_message_to_stderr("CRITICAL ERROR: Logging system failure: " + std::string(critical_exception_error.what()));
        std::cerr << message << std::endl;
    } catch (...) {
        log_message_to_stderr("CRITICAL ERROR: Unknown logging system failure");
        std::cerr << message << std::endl;
    }
}

void log_message_to_stderr(const std::string& error_message) {
    std::cerr << error_message << std::endl;
}


void log_inline_status(const std::string& message) {
    try {
        LoggingContext* thread_logging_context_ptr = get_logging_context();
        std::lock_guard<std::mutex> console_guard(thread_logging_context_ptr->console_mutex);
        std::cout << "\r" << message << std::flush;
        thread_logging_context_ptr->inline_active.store(true);
    } catch (const std::exception& exception_error) {
        log_message_to_stderr("ERROR: Failed to log inline status: " + std::string(exception_error.what()));
    } catch (...) {
        log_message_to_stderr("ERROR: Unknown inline status logging failure");
    }
}

void end_inline_status() {
    try {
        LoggingContext* thread_logging_context_ptr = get_logging_context();
        std::lock_guard<std::mutex> console_guard(thread_logging_context_ptr->console_mutex);
        if (thread_logging_context_ptr->inline_active.load()) {
            std::cout << std::endl;
            thread_logging_context_ptr->inline_active.store(false);
        }
    } catch (const std::exception& exception_error) {
        log_message_to_stderr("ERROR: Failed to end inline status: " + std::string(exception_error.what()));
    } catch (...) {
        log_message_to_stderr("ERROR: Unknown end inline status failure");
    }
}

std::string get_formatted_inline_message(const std::string& content) {
    try {
        LoggingContext* thread_logging_context_ptr = get_logging_context();
        std::string timestamp_string = TimeUtils::get_current_human_readable_time();
        std::string thread_tag_string = thread_logging_context_ptr->get_thread_tag();
        std::stringstream formatted_stream;
        formatted_stream << timestamp_string << " [" << thread_tag_string << "]   " << content;
        return formatted_stream.str();
    } catch (const std::exception& exception_error) {
        log_message_to_stderr("ERROR: Failed to format inline message: " + std::string(exception_error.what()));
        return content;
    } catch (...) {
        log_message_to_stderr("ERROR: Unknown inline message formatting failure");
        return content;
    }
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
    } catch (const std::exception& filesystem_exception_error) {
        log_message(std::string("CRITICAL ERROR: Failed to create run folder: ") + filesystem_exception_error.what(), "");
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

void initialize_global_logger(AsyncLogger& logger_instance) {
    LoggingContext* thread_logging_context_ptr = get_logging_context();
    if (!thread_logging_context_ptr->async_logger) {
        throw std::runtime_error("Async logger not set in context before initialization");
    }
    if (thread_logging_context_ptr->async_logger.get() != &logger_instance) {
        throw std::runtime_error("Async logger mismatch in context");
    }
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

// Message processing implementations
void AsyncLogger::collect_all_available_messages_internal(std::vector<std::string>& message_buffer) {
    // Collect all available messages immediately (no waiting)
    while (!queue.empty()) {
        message_buffer.push_back(std::move(queue.front()));
        queue.pop();
    }
}

void AsyncLogger::collect_all_available_messages(std::vector<std::string>& message_buffer) {
    std::unique_lock<std::mutex> lock(mtx);
    collect_all_available_messages_internal(message_buffer);
}

void AsyncLogger::output_log_line_internal(const std::string& log_line, std::ofstream& log_file) {
    {
        LoggingContext* thread_logging_context_ptr = get_logging_context();
        std::lock_guard<std::mutex> cguard(thread_logging_context_ptr->console_mutex);
        if (thread_logging_context_ptr->inline_active.load()) {
            std::cout << std::endl;
            thread_logging_context_ptr->inline_active.store(false);
        }
        std::cout << log_line << std::flush;
    }
    
    if (log_file.is_open()) {
        log_file << log_line;
        log_file.flush();
    }
}

void AsyncLogger::write_buffered_messages_to_log(const std::vector<std::string>& message_buffer, std::ofstream& log_file) {
    // Write all buffered messages to console and log file
    for (const auto& log_line : message_buffer) {
        output_log_line_internal(log_line, log_file);
    }
}

void AsyncLogger::flush_message_buffer(std::vector<std::string>& message_buffer, std::ofstream& log_file) {
    // Write messages to log and clear buffer
    write_buffered_messages_to_log(message_buffer, log_file);
    message_buffer.clear();
}

void AsyncLogger::process_logging_queue_with_timeout(std::ofstream& log_file, int poll_interval_seconds) {
    std::unique_lock<std::mutex> lock(mtx);
    
    // Wait for messages with configurable timeout to allow periodic checks for thread shutdown
    auto timeout_duration = std::chrono::milliseconds(poll_interval_seconds * 100); // 10% of logging interval
    cv.wait_for(lock, timeout_duration, [&]{ return !queue.empty() || !running.load(); });
    
    // Process all available messages
    while (!queue.empty()) {
        std::string line = std::move(queue.front());
        queue.pop();
        lock.unlock();
        
        output_log_line_internal(line, log_file);
        
        lock.lock();
    }
}

void AsyncLogger::process_logging_queue(std::ofstream& log_file) {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&]{ return !queue.empty() || !running.load(); });
    
    while (!queue.empty()) {
        std::string line = std::move(queue.front());
        queue.pop();
        lock.unlock();
        
        output_log_line_internal(line, log_file);
        
        lock.lock();
    }
}

std::shared_ptr<AsyncLogger> initialize_application_foundation(const AlpacaTrader::Config::SystemConfig& config) {
    LoggingContext* thread_logging_context_ptr = get_logging_context();

    thread_logging_context_ptr->run_folder = create_unique_run_folder();

    std::string base_filename_string = thread_logging_context_ptr->run_folder + "/" + extract_base_filename(config.logging.log_file);
    std::string timestamped_log_filename = generate_timestamped_log_filename(base_filename_string);

    auto logger_instance = std::make_shared<AsyncLogger>(timestamped_log_filename);

    std::string configuration_error_message;
    if (!validate_config(config, configuration_error_message)) {
        log_message_to_stderr("ERROR: Config error: " + configuration_error_message);
        throw std::runtime_error("Configuration validation failed: " + configuration_error_message);
    }

    thread_logging_context_ptr->async_logger = logger_instance;
    initialize_global_logger(*logger_instance);
    set_log_thread_tag("MAIN  ");

    return logger_instance;
}

std::shared_ptr<CSVBarsLogger> initialize_csv_bars_logger(const std::string& base_filename) {
    try {
        LoggingContext* thread_logging_context_ptr = get_logging_context();
        
        if (thread_logging_context_ptr->run_folder.empty()) {
            throw std::runtime_error("Run folder not initialized - call initialize_application_foundation first");
        }

        std::string bars_filename_string = thread_logging_context_ptr->run_folder + "/" + extract_base_filename(base_filename) + "_bars";
        std::string timestamped_bars_filename_string = generate_timestamped_log_filename(bars_filename_string);
        auto bars_logger_instance = std::make_shared<CSVBarsLogger>(timestamped_bars_filename_string);

        if (!bars_logger_instance->is_initialized()) {
            throw std::runtime_error("Failed to initialize CSV bars logger");
        }

        thread_logging_context_ptr->csv_bars_logger = bars_logger_instance;
        return bars_logger_instance;
    } catch (const std::exception& exception_error) {
        log_message_to_stderr(std::string("CRITICAL ERROR: Failed to initialize CSV bars logger: ") + exception_error.what());
        throw;
    } catch (...) {
        log_message_to_stderr("CRITICAL ERROR: Unknown error initializing CSV bars logger");
        throw std::runtime_error("Failed to initialize CSV bars logger");
    }
}

std::shared_ptr<CSVTradeLogger> initialize_csv_trade_logger(const std::string& base_filename) {
    try {
        LoggingContext* thread_logging_context_ptr = get_logging_context();
        
        if (thread_logging_context_ptr->run_folder.empty()) {
            throw std::runtime_error("Run folder not initialized - call initialize_application_foundation first");
        }

        std::string trade_filename_string = thread_logging_context_ptr->run_folder + "/" + extract_base_filename(base_filename) + "_trades";
        std::string timestamped_trade_filename_string = generate_timestamped_log_filename(trade_filename_string);
        auto trade_logger_instance = std::make_shared<CSVTradeLogger>(timestamped_trade_filename_string);

        if (!trade_logger_instance->is_valid()) {
            throw std::runtime_error("Failed to initialize CSV trade logger");
        }

        thread_logging_context_ptr->csv_trade_logger = trade_logger_instance;
        return trade_logger_instance;
    } catch (const std::exception& exception_error) {
        log_message_to_stderr(std::string("CRITICAL ERROR: Failed to initialize CSV trade logger: ") + exception_error.what());
        throw;
    } catch (...) {
        log_message_to_stderr("CRITICAL ERROR: Unknown error initializing CSV trade logger");
        throw std::runtime_error("Failed to initialize CSV trade logger");
    }
}

void set_logging_context(LoggingContext& context) {
    thread_local_logging_context_pointer = &context;
}

} // namespace Logging
} // namespace AlpacaTrader
