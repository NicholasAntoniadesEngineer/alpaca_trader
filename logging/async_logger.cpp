/**
 * Asynchronous logging system for high-performance trading operations.
 */
#include "async_logger.hpp"
#include "../threads/platform/thread_control.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <ctime>

static AsyncLogger* g_async_logger = nullptr;
static thread_local std::string t_log_tag = "MAIN  ";
std::mutex g_console_mtx;
std::atomic<bool> g_inline_active{false};

void set_async_logger(AsyncLogger* logger) {
    g_async_logger = logger;
}

void set_log_thread_tag(const std::string& tag6) {
    std::string t = tag6;
    if (t.size() < LOG_TAG_WIDTH) t.append(LOG_TAG_WIDTH - t.size(), ' ');
    if (t.size() > LOG_TAG_WIDTH) t = t.substr(0, LOG_TAG_WIDTH);
    t_log_tag = t;
}


void log_message(const std::string& message, const std::string& log_file_path) {
    std::time_t now = std::time(nullptr);
    std::tm* local_tm = std::localtime(&now);
    std::stringstream ss;
    ss << std::put_time(local_tm, "%Y-%m-%d %H:%M:%S") << " [" << t_log_tag << "]   " << message << std::endl;
    std::string log_str = ss.str();

    if (g_async_logger) {
        g_async_logger->enqueue(log_str);
        return;
    }

    {
        std::lock_guard<std::mutex> cguard(g_console_mtx);
        if (g_inline_active.load()) {
            std::cout << std::endl;
            g_inline_active.store(false);
        }
        std::cout << log_str;
    }
    std::ofstream log_file(log_file_path, std::ios::app);
    if (log_file.is_open()) {
        log_file << log_str;
        log_file.close();
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
    std::time_t now = std::time(nullptr);
    std::tm* local_tm = std::localtime(&now);
    std::stringstream ss;
    ss << std::put_time(local_tm, "%Y-%m-%d %H:%M:%S") << " [" << t_log_tag << "]   " << content;
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
