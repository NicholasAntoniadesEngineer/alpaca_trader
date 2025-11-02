#ifndef ACCOUNT_DATA_THREAD_LOGS_HPP
#define ACCOUNT_DATA_THREAD_LOGS_HPP

#include "configs/system_config.hpp"
#include <string>
#include <atomic>

namespace AlpacaTrader {
namespace Logging {

class AccountDataThreadLogs {
public:
    // Thread lifecycle logging
    static void log_thread_exception(const std::string& error_message);
    static void log_thread_loop_exception(const std::string& error_message);
    static void log_thread_collection_loop_start();
    static void log_fetch_allowed_check();
    static void log_before_fetch_account_data();
    static void log_iteration_complete();
    
    // Gate checking logging
    static void log_allow_fetch_ptr_null();
    static void log_fetch_not_allowed_by_gate();
    static void log_is_fetch_allowed_exception(const std::string& error_message);
    
    // Utility functions
    static bool is_fetch_allowed(const std::atomic<bool>* allow_fetch_ptr);
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // ACCOUNT_DATA_THREAD_LOGS_HPP

