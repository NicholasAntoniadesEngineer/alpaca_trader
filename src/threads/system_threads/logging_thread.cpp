#include "logging_thread.hpp"
#include "logging/async_logger.hpp"
#include "logging/startup_logs.hpp"
#include "../thread_logic/thread_registry.hpp"
#include "../thread_logic/platform/thread_control.hpp"
#include <iostream>
#include <fstream>
#include <memory>
#include <chrono>

using AlpacaTrader::Threads::LoggingThread;
using AlpacaTrader::Logging::set_log_thread_tag;
using AlpacaTrader::Logging::log_message;
using AlpacaTrader::Logging::g_console_mtx;
using AlpacaTrader::Logging::g_inline_active;


void AlpacaTrader::Threads::LoggingThread::operator()() {
    
    setup_logging_thread();
    
    // Wait for main thread to complete priority setup
    std::this_thread::sleep_for(std::chrono::milliseconds(config.timing.thread_startup_delay_ms));
    
    
    logging_loop();
    
    log_message("LoggingThread exited", "trading_system.log");
}

void AlpacaTrader::Threads::LoggingThread::setup_logging_thread() {
    AlpacaTrader::Config::ThreadSettings thread_config = AlpacaTrader::Core::ThreadRegistry::get_config_for_type(AlpacaTrader::Core::ThreadRegistry::Type::LOGGING, config);
    ThreadSystem::Platform::ThreadControl::set_current_priority(thread_config);
    
    set_log_thread_tag("LOGGER");
}

void AlpacaTrader::Threads::LoggingThread::logging_loop() {
    std::ofstream log_file(logger_ptr->get_file_path(), std::ios::app);
    logger_ptr->running.store(true);

    while (logger_ptr->running.load()) {
        process_logging_queue(log_file);
    }
}

void AlpacaTrader::Threads::LoggingThread::process_logging_queue(std::ofstream& log_file) {
    std::unique_lock<std::mutex> lock(logger_ptr->mtx);
    logger_ptr->cv.wait(lock, [&]{ return !logger_ptr->queue.empty() || !logger_ptr->running.load(); });
    
    while (!logger_ptr->queue.empty()) {
        std::string line = std::move(logger_ptr->queue.front());
        logger_ptr->queue.pop();
        logger_iterations++;
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
