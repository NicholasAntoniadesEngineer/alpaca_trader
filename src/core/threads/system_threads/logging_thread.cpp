/**
 * Logging thread.
 * Handles asynchronous logging operations and console output management.
 */
#include "logging_thread.hpp"
#include "core/logging/logger/async_logger.hpp"
#include "core/logging/logs/startup_logs.hpp"
#include "core/threads/thread_logic/thread_registry.hpp"
#include "core/threads/thread_logic/platform/thread_control.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <vector>

// Using declarations for cleaner code
using namespace AlpacaTrader::Threads;
using namespace AlpacaTrader::Logging;
using namespace AlpacaTrader::Config;

// ========================================================================
// THREAD LIFECYCLE MANAGEMENT
// ========================================================================

void LoggingThread::operator()() {
    try {
        setup_logging_thread();
        
        // Wait for main thread to complete priority setup
        std::this_thread::sleep_for(std::chrono::milliseconds(config.timing.thread_startup_sequence_delay_milliseconds));
        
        // Start the logging processing loop
        execute_logging_processing_loop();
        
        log_message("LoggingThread exited", "trading_system.log");
    } catch (const std::exception& exception) {
        log_message("LoggingThread exception: " + std::string(exception.what()), "trading_system.log");
    } catch (...) {
        log_message("LoggingThread unknown exception", "trading_system.log");
    }
}

void LoggingThread::setup_logging_thread() {
    ThreadSettings thread_config = ThreadRegistry::get_config_for_type(ThreadRegistry::Type::LOGGING, config);
    ::ThreadSystem::Platform::ThreadControl::set_current_priority(thread_config);
    
    set_log_thread_tag("LOGGER");
}

void LoggingThread::execute_logging_processing_loop() {
    try {
        std::ofstream log_file(logger_ptr->get_file_path(), std::ios::app);
        logger_ptr->running.store(true);

        std::vector<std::string> message_buffer;
        auto last_flush_time = std::chrono::steady_clock::now();

        while (logger_ptr->running.load()) {
            try {
                // Collect all available messages immediately (don't wait)
                collect_all_available_messages(message_buffer);

                auto current_time = std::chrono::steady_clock::now();
                auto time_since_flush = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_flush_time);

                // Only flush every configured interval, regardless of buffer size
                if (time_since_flush.count() >= config.timing.thread_logging_poll_interval_sec) {
                    if (!message_buffer.empty()) {
                        flush_message_buffer(message_buffer, log_file);
                        last_flush_time = current_time;
                        logger_iterations->fetch_add(1); // Only increment when we actually write
                    }
                }

                // Sleep for configured interval between checks - maintains responsiveness based on config
                std::this_thread::sleep_for(std::chrono::seconds(config.timing.thread_logging_poll_interval_sec));
            } catch (const std::exception& exception) {
                log_message("LoggingThread loop iteration exception: " + std::string(exception.what()), "trading_system.log");
                std::this_thread::sleep_for(std::chrono::seconds(config.timing.thread_logging_poll_interval_sec));
            } catch (...) {
                log_message("LoggingThread loop iteration unknown exception", "trading_system.log");
                std::this_thread::sleep_for(std::chrono::seconds(config.timing.thread_logging_poll_interval_sec));
            }
        }

        // Final flush of any remaining messages
        if (!message_buffer.empty()) {
            flush_message_buffer(message_buffer, log_file);
        }
    } catch (const std::exception& exception) {
        log_message("LoggingThread logging_loop exception: " + std::string(exception.what()), "trading_system.log");
    } catch (...) {
        log_message("LoggingThread logging_loop unknown exception", "trading_system.log");
    }
}

// ========================================================================
// LOGGING PROCESSING
// ========================================================================

void LoggingThread::collect_all_available_messages(std::vector<std::string>& message_buffer) {
    std::unique_lock<std::mutex> lock(logger_ptr->mtx);

    // Collect all available messages immediately (no waiting)
    while (!logger_ptr->queue.empty()) {
        message_buffer.push_back(std::move(logger_ptr->queue.front()));
        logger_ptr->queue.pop();
    }
}

void LoggingThread::collect_messages_for_batch(std::vector<std::string>& message_buffer) {
    std::unique_lock<std::mutex> lock(logger_ptr->mtx);

    // Wait for messages with configurable timeout (use 10% of logging interval)
    auto timeout_duration = std::chrono::milliseconds(config.timing.thread_logging_poll_interval_sec * 100);
    logger_ptr->cv.wait_for(lock, timeout_duration, [&]{ return !logger_ptr->queue.empty() || !logger_ptr->running.load(); });

    // Collect all available messages into the buffer
    while (!logger_ptr->queue.empty()) {
        message_buffer.push_back(std::move(logger_ptr->queue.front()));
        logger_ptr->queue.pop();
    }
}

void LoggingThread::flush_message_buffer(std::vector<std::string>& message_buffer, std::ofstream& log_file) {
    // Output all buffered messages
    for (const auto& log_line : message_buffer) {
        output_log_line(log_line, log_file);
    }

    // Clear the buffer
    message_buffer.clear();
}

void LoggingThread::process_logging_queue_with_timeout(std::ofstream& log_file) {
    std::unique_lock<std::mutex> lock(logger_ptr->mtx);

    // Wait for messages with configurable timeout to allow periodic checks for thread shutdown
    auto timeout_duration = std::chrono::milliseconds(config.timing.thread_logging_poll_interval_sec * 100); // 10% of logging interval
    logger_ptr->cv.wait_for(lock, timeout_duration, [&]{ return !logger_ptr->queue.empty() || !logger_ptr->running.load(); });

    // Process all available messages
    while (!logger_ptr->queue.empty()) {
        std::string line = std::move(logger_ptr->queue.front());
        logger_ptr->queue.pop();
        lock.unlock();

        output_log_line(line, log_file);

        lock.lock();
    }
}

void LoggingThread::process_logging_queue(std::ofstream& log_file) {
    std::unique_lock<std::mutex> lock(logger_ptr->mtx);
    logger_ptr->cv.wait(lock, [&]{ return !logger_ptr->queue.empty() || !logger_ptr->running.load(); });

    while (!logger_ptr->queue.empty()) {
        std::string line = std::move(logger_ptr->queue.front());
        logger_ptr->queue.pop();
        lock.unlock();

        output_log_line(line, log_file);

        lock.lock();
    }
}

// ========================================================================
// OUTPUT PROCESSING
// ========================================================================

void LoggingThread::output_log_line(const std::string& log_line, std::ofstream& log_file) {
    {
        std::lock_guard<std::mutex> cguard(get_console_mutex());
        if (get_inline_active_flag().load()) {
            std::cout << std::endl;
            get_inline_active_flag().store(false);
        }
        std::cout << log_line << std::flush;
    }
    
    if (log_file.is_open()) {
        log_file << log_line;
        log_file.flush();
    }
}
