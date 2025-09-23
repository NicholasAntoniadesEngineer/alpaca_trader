#ifndef LOGGING_THREAD_HPP
#define LOGGING_THREAD_HPP

#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <fstream>
#include <memory>

// Forward declaration
class AsyncLogger;

class LoggingThread {
private:
    std::shared_ptr<AsyncLogger> logger_ptr;
    std::atomic<unsigned long>& logger_iterations;

public:
    LoggingThread(std::shared_ptr<AsyncLogger> logger,
                  std::atomic<unsigned long>& iterations)
        : logger_ptr(logger), logger_iterations(iterations) {}

    void operator()();
};

#endif // LOGGING_THREAD_HPP
