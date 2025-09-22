#ifndef LOGGING_THREAD_HPP
#define LOGGING_THREAD_HPP

#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <fstream>

class LoggingThread {
private:
    std::string file_path;
    std::mutex& mtx;
    std::condition_variable& cv;
    std::queue<std::string>& queue;
    std::atomic<bool>& running;
    std::atomic<unsigned long>& logger_iterations;

public:
    LoggingThread(const std::string& log_file_path,
                std::mutex& m,
                std::condition_variable& c,
                std::queue<std::string>& q,
                std::atomic<bool>& r,
                std::atomic<unsigned long>& iterations)
        : file_path(log_file_path), mtx(m), cv(c), queue(q), running(r), logger_iterations(iterations) {}

    void operator()();
};

#endif // LOGGING_THREAD_HPP
