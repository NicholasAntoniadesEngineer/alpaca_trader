#include "threads/logging_thread.hpp"
#include "logging/async_logger.hpp"
#include "logging/startup_logger.hpp"
#include "configs/thread_config.hpp"
#include "platform/thread_control.hpp"
#include <iostream>
#include <fstream>
#include <memory>

// Using declarations for cleaner code
using AlpacaTrader::Threads::LoggingThread;
using AlpacaTrader::Logging::set_log_thread_tag;
using AlpacaTrader::Logging::g_console_mtx;
using AlpacaTrader::Logging::g_inline_active;


void LoggingThread::operator()() {
    AlpacaTrader::Config::ThreadConfig config = AlpacaTrader::Config::ConfigProvider::get_default_config(AlpacaTrader::Config::Type::LOGGING);
    ThreadSystem::Platform::ThreadControl::set_current_priority(config);
    
    set_log_thread_tag("LOGGER");

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    std::ofstream log_file(logger_ptr->get_file_path(), std::ios::app);
    logger_ptr->running.store(true);
    
    while (logger_ptr->running.load()) {
        std::unique_lock<std::mutex> lock(logger_ptr->mtx);
        logger_ptr->cv.wait(lock, [&]{ return !logger_ptr->queue.empty() || !logger_ptr->running.load(); });
        
        while (!logger_ptr->queue.empty()) {
            std::string line = std::move(logger_ptr->queue.front());
            logger_ptr->queue.pop();
            logger_iterations++;
            lock.unlock();
            
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
            
            lock.lock();
        }
    }
}
