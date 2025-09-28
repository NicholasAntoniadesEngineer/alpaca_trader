#include "logging_thread.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/startup_logs.hpp"
#include "../thread_logic/thread_registry.hpp"
#include "../thread_logic/platform/thread_control.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

using AlpacaTrader::Threads::LoggingThread;
using AlpacaTrader::Logging::set_log_thread_tag;
using AlpacaTrader::Logging::log_message;
using AlpacaTrader::Logging::g_console_mtx;
using AlpacaTrader::Logging::g_inline_active;


void AlpacaTrader::Threads::LoggingThread::operator()() {
    try {
        setup_logging_thread();
        
        // Wait for main thread to complete priority setup
        std::this_thread::sleep_for(std::chrono::milliseconds(config.timing.thread_startup_delay_ms));
        
        
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

        while (logger_ptr->running.load()) {
            try {
                process_logging_queue(log_file);
                logger_iterations->fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small delay to prevent busy waiting
            } catch (const std::exception& e) {
                log_message("LoggingThread loop iteration exception: " + std::string(e.what()), "trading_system.log");
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } catch (...) {
                log_message("LoggingThread loop iteration unknown exception", "trading_system.log");
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    } catch (const std::exception& e) {
        log_message("LoggingThread logging_loop exception: " + std::string(e.what()), "trading_system.log");
    } catch (...) {
        log_message("LoggingThread logging_loop unknown exception", "trading_system.log");
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
        std::cout << line;
    }
    
    if (log_file.is_open()) {
        log_file << line;
        log_file.flush();
    }
}
