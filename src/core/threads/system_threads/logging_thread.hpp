#ifndef LOGGING_THREAD_HPP
#define LOGGING_THREAD_HPP

#include <string>
#include <atomic>
#include <fstream>
#include <memory>
#include <vector>
#include "core/logging/async_logger.hpp"
#include "configs/system_config.hpp"

namespace AlpacaTrader {
namespace Threads {

class LoggingThread {
public:
    LoggingThread(std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger,
                  std::atomic<unsigned long>& iterations,
                  const AlpacaTrader::Config::SystemConfig& system_config)
        : logger_ptr(logger), logger_iterations(&iterations), config(system_config) {}

    void operator()();
    void set_iteration_counter(std::atomic<unsigned long>& counter) { logger_iterations = &counter; }

private:
    std::shared_ptr<AlpacaTrader::Logging::AsyncLogger> logger_ptr;
    std::atomic<unsigned long>* logger_iterations;
    const AlpacaTrader::Config::SystemConfig& config;

    void setup_logging_thread();
    void execute_logging_processing_loop();
    void collect_all_available_messages(std::vector<std::string>& message_buffer);
    void collect_messages_for_batch(std::vector<std::string>& message_buffer);
    void flush_message_buffer(std::vector<std::string>& message_buffer, std::ofstream& log_file);
    void process_logging_queue_with_timeout(std::ofstream& log_file);
    void process_logging_queue(std::ofstream& log_file);
    void output_log_line(const std::string& log_line, std::ofstream& log_file);
};

} // namespace Threads
} // namespace AlpacaTrader

#endif // LOGGING_THREAD_HPP
