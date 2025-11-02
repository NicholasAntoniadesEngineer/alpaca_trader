#ifndef LOGGING_THREAD_LOGS_HPP
#define LOGGING_THREAD_LOGS_HPP

#include <string>

namespace AlpacaTrader {
namespace Logging {

class LoggingThreadLogs {
public:
    // Thread lifecycle logging
    static void log_thread_exception(const std::string& error_message);
    static void log_thread_exited();
    
    // Loop logging
    static void log_loop_iteration_exception(const std::string& error_message);
    static void log_loop_iteration_unknown_exception();
    static void log_logging_loop_exception(const std::string& error_message);
    static void log_logging_loop_unknown_exception();
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // LOGGING_THREAD_LOGS_HPP

