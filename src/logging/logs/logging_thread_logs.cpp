#include "logging_thread_logs.hpp"
#include "logging/logger/async_logger.hpp"

using namespace AlpacaTrader::Logging;

void LoggingThreadLogs::log_thread_exception(const std::string& error_message) {
    log_message("LoggingThread exception: " + error_message, "trading_system.log");
}

void LoggingThreadLogs::log_thread_exited() {
    log_message("LoggingThread exited", "trading_system.log");
}

void LoggingThreadLogs::log_loop_iteration_exception(const std::string& error_message) {
    log_message("LoggingThread loop iteration exception: " + error_message, "trading_system.log");
}

void LoggingThreadLogs::log_loop_iteration_unknown_exception() {
    log_message("LoggingThread loop iteration unknown exception", "trading_system.log");
}

void LoggingThreadLogs::log_logging_loop_exception(const std::string& error_message) {
    log_message("LoggingThread logging_loop exception: " + error_message, "trading_system.log");
}

void LoggingThreadLogs::log_logging_loop_unknown_exception() {
    log_message("LoggingThread logging_loop unknown exception", "trading_system.log");
}

