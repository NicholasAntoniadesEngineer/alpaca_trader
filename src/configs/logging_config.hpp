// LoggingConfig.hpp
#ifndef LOGGING_CONFIG_HPP
#define LOGGING_CONFIG_HPP

#include <string>

struct LoggingConfig {
    std::string log_file;
    int max_log_file_size_mb;
    int log_backup_count;
    std::string console_log_level;
    std::string file_log_level;
    bool include_timestamp;
    bool include_thread_id;
    bool include_function_name;
};

#endif // LOGGING_CONFIG_HPP


