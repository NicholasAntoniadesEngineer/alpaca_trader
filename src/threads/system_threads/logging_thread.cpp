/**
 * Logging thread.
 * Handles asynchronous logging operations and console output management.
 */
#include "logging_thread.hpp"
#include "logging/logger/async_logger.hpp"
#include "logging/logs/logging_thread_logs.hpp"
#include "threads/thread_logic/thread_registry.hpp"
#include "threads/thread_logic/platform/thread_control.hpp"
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
        
        LoggingThreadLogs::log_thread_exited();
    } catch (const std::exception& exception) {
        LoggingThreadLogs::log_thread_exception(exception.what());
    } catch (...) {
        LoggingThreadLogs::log_thread_exception("Unknown error");
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
                logger_ptr->collect_all_available_messages(message_buffer);

                auto current_time = std::chrono::steady_clock::now();
                auto time_since_flush = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_flush_time);

                // Only flush every configured interval, regardless of buffer size
                if (time_since_flush.count() >= config.timing.thread_logging_poll_interval_sec) {
                    if (!message_buffer.empty()) {
                        logger_ptr->flush_message_buffer(message_buffer, log_file);
                        last_flush_time = current_time;
                        logger_iterations->fetch_add(1); // Only increment when we actually write
                    }
                }

                // Sleep for configured interval between checks - maintains responsiveness based on config
                std::this_thread::sleep_for(std::chrono::seconds(config.timing.thread_logging_poll_interval_sec));
            } catch (const std::exception& exception) {
                LoggingThreadLogs::log_loop_iteration_exception(exception.what());
                std::this_thread::sleep_for(std::chrono::seconds(config.timing.thread_logging_poll_interval_sec));
            } catch (...) {
                LoggingThreadLogs::log_loop_iteration_unknown_exception();
                std::this_thread::sleep_for(std::chrono::seconds(config.timing.thread_logging_poll_interval_sec));
            }
        }

        // Final flush of any remaining messages
        if (!message_buffer.empty()) {
            logger_ptr->flush_message_buffer(message_buffer, log_file);
        }
    } catch (const std::exception& exception) {
        LoggingThreadLogs::log_logging_loop_exception(exception.what());
    } catch (...) {
        LoggingThreadLogs::log_logging_loop_unknown_exception();
    }
}
