#include "logging_thread.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/startup_logs.hpp"
#include "core/threads/thread_logic/thread_registry.hpp"
#include "core/threads/thread_logic/platform/thread_control.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <vector>

using AlpacaTrader::Threads::LoggingThread;
using AlpacaTrader::Logging::set_log_thread_tag;
using AlpacaTrader::Logging::log_message;
using AlpacaTrader::Logging::g_console_mtx;
using AlpacaTrader::Logging::g_inline_active;


void AlpacaTrader::Threads::LoggingThread::operator()() {
    try {
        setup_logging_thread();
        
        // Wait for main thread to complete priority setup
        std::this_thread::sleep_for(std::chrono::milliseconds(config.timing.thread_startup_sequence_delay_milliseconds));
        
        
        logging_loop();
        
        log_message("LoggingThread exited", "trading_system.log");
    } catch (const std::exception& e) {
        log_message("LoggingThread exception: " + std::string(e.what()), "trading_system.log");
    } catch (...) {
        log_message("LoggingThread unknown exception", "trading_system.log");
    }
}

void AlpacaTrader::Threads::LoggingThread::setup_logging_thread() {
    AlpacaTrader::Config::ThreadSettings thread_config = AlpacaTrader::Core::ThreadRegistry::get_config_for_type(AlpacaTrader::Core::ThreadRegistry::Type::LOGGING, config);
    ThreadSystem::Platform::ThreadControl::set_current_priority(thread_config);
    
    set_log_thread_tag("LOGGER");
}

void AlpacaTrader::Threads::LoggingThread::logging_loop() {
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
            } catch (const std::exception& e) {
                log_message("LoggingThread loop iteration exception: " + std::string(e.what()), "trading_system.log");
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
    } catch (const std::exception& e) {
        log_message("LoggingThread logging_loop exception: " + std::string(e.what()), "trading_system.log");
    } catch (...) {
        log_message("LoggingThread logging_loop unknown exception", "trading_system.log");
    }
}

void AlpacaTrader::Threads::LoggingThread::collect_all_available_messages(std::vector<std::string>& buffer) {
    std::unique_lock<std::mutex> lock(logger_ptr->mtx);

    // Collect all available messages immediately (no waiting)
    while (!logger_ptr->queue.empty()) {
        buffer.push_back(std::move(logger_ptr->queue.front()));
        logger_ptr->queue.pop();
    }
}

void AlpacaTrader::Threads::LoggingThread::collect_messages_for_batch(std::vector<std::string>& buffer) {
    std::unique_lock<std::mutex> lock(logger_ptr->mtx);

    // Wait for messages with configurable timeout (use 10% of logging interval)
    auto timeout_duration = std::chrono::milliseconds(config.timing.thread_logging_poll_interval_sec * 100);
    logger_ptr->cv.wait_for(lock, timeout_duration, [&]{ return !logger_ptr->queue.empty() || !logger_ptr->running.load(); });

    // Collect all available messages into the buffer
    while (!logger_ptr->queue.empty()) {
        buffer.push_back(std::move(logger_ptr->queue.front()));
        logger_ptr->queue.pop();
    }
}

void AlpacaTrader::Threads::LoggingThread::flush_message_buffer(std::vector<std::string>& buffer, std::ofstream& log_file) {
    // Output all buffered messages
    for (const auto& line : buffer) {
        output_log_line(line, log_file);
    }

    // Clear the buffer
    buffer.clear();
}

void AlpacaTrader::Threads::LoggingThread::process_logging_queue_with_timeout(std::ofstream& log_file) {
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

void AlpacaTrader::Threads::LoggingThread::process_logging_queue(std::ofstream& log_file) {
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

void AlpacaTrader::Threads::LoggingThread::output_log_line(const std::string& line, std::ofstream& log_file) {
    {
        std::lock_guard<std::mutex> cguard(g_console_mtx);
        if (g_inline_active.load()) {
            std::cout << std::endl;
            g_inline_active.store(false);
        }
        std::cout << line << std::flush;
    }
    
    if (log_file.is_open()) {
        log_file << line;
        log_file.flush();
    }
}
