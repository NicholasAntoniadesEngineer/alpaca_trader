#include "account_data_thread_logs.hpp"
#include "logging/logger/async_logger.hpp"

using namespace AlpacaTrader::Logging;

void AccountDataThreadLogs::log_thread_exception(const std::string& error_message) {
    log_message("AccountDataThread exception: " + error_message, "trading_system.log");
}

void AccountDataThreadLogs::log_thread_loop_exception(const std::string& error_message) {
    log_message("AccountDataThread loop iteration exception: " + error_message, "trading_system.log");
}

void AccountDataThreadLogs::log_thread_collection_loop_start() {
    log_message("AccountDataThread entered execute_account_data_collection_loop", "trading_system.log");
}

void AccountDataThreadLogs::log_fetch_allowed_check() {
    log_message("AccountDataThread before is_fetch_allowed", "trading_system.log");
}

void AccountDataThreadLogs::log_before_fetch_account_data() {
    log_message("AccountDataThread before fetch_and_update_account_data", "trading_system.log");
}

void AccountDataThreadLogs::log_iteration_complete() {
    log_message("AccountDataThread iteration complete - sleeping", "trading_system.log");
}

void AccountDataThreadLogs::log_allow_fetch_ptr_null() {
    log_message("AccountDataThread allow_fetch_ptr is null", "trading_system.log");
}

void AccountDataThreadLogs::log_fetch_not_allowed_by_gate() {
    log_message("AccountDataThread fetch not allowed by gate", "trading_system.log");
}

void AccountDataThreadLogs::log_is_fetch_allowed_exception(const std::string& error_message) {
    log_message("AccountDataThread is_fetch_allowed exception: " + error_message, "trading_system.log");
}

bool AccountDataThreadLogs::is_fetch_allowed(const std::atomic<bool>* allow_fetch_ptr) {
    try {
        if (!allow_fetch_ptr) {
            log_allow_fetch_ptr_null();
            return false;
        }
        bool allowed = allow_fetch_ptr->load();
        if (!allowed) {
            log_fetch_not_allowed_by_gate();
        }
        return allowed;
    } catch (const std::exception& exception_error) {
        log_is_fetch_allowed_exception(exception_error.what());
        return false;
    } catch (...) {
        log_is_fetch_allowed_exception("Unknown error");
        return false;
    }
}

