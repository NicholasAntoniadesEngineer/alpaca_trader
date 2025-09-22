#include "logging_thread.hpp"
#include "../logging/async_logger.hpp"
#include "../logging/startup_logger.hpp"
#include "config/thread_config.hpp"
#include "platform/thread_control.hpp"
#include <iostream>
#include <fstream>

extern std::mutex g_console_mtx;
extern std::atomic<bool> g_inline_active;

void LoggingThread::operator()() {
    ThreadSystem::ThreadConfig config = ThreadSystem::ConfigProvider::get_default_config(ThreadSystem::Type::LOGGING);
    ThreadSystem::Platform::ThreadControl::set_current_priority(config);
    
    set_log_thread_tag("LOGGER");

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    std::ofstream log_file(file_path, std::ios::app);
    running.store(true);
    
    while (running.load()) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]{ return !queue.empty() || !running.load(); });
        
        while (!queue.empty()) {
            std::string line = std::move(queue.front());
            queue.pop();
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
