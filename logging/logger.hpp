#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <memory>
#include <thread>

/**
 * High-performance centralized logging system for trading operations.
 * Thread-safe asynchronous logging with minimal overhead on critical trading paths.
 */
class Logger {
public:
    enum class Level { DEBUG, INFO, WARN, ERROR };
    
    static Logger& instance();
    
    void init(const std::string& log_file_path);
    void shutdown();
    
    // Core logging functions - optimized for performance
    void log(Level level, const std::string& tag, const std::string& message);
    void info(const std::string& tag, const std::string& message);
    void warn(const std::string& tag, const std::string& message);
    void error(const std::string& tag, const std::string& message);
    
    // Trading-specific optimized functions
    void log_trade_signal(const std::string& symbol, const std::string& signal, double price);
    void log_trade_execution(const std::string& order_id, const std::string& side, int qty, double price);
    void log_account_update(double equity, double buying_power);
    void log_market_data(const std::string& symbol, double price, double volume);
    
    // Performance monitoring
    void log_thread_performance(const std::string& thread_name, const std::string& metrics);
    void log_system_status(const std::string& status, const std::string& details = "");
    
    // Inline status for UI (non-blocking, overwrites console line)
    void set_inline_status(const std::string& message);
    void clear_inline_status();
    
    // Thread management
    void set_thread_tag(const std::string& tag);
    std::string get_thread_info() const;
    
private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    struct LogEntry {
        Level level;
        std::string timestamp;
        std::string tag;
        std::string message;
        std::string formatted_line;
    };
    
    void worker_thread();
    void enqueue_entry(Level level, const std::string& tag, const std::string& message);
    std::string format_timestamp() const;
    std::string format_entry(const LogEntry& entry) const;
    
    std::string file_path_;
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<LogEntry> log_queue_;
    
    mutable std::mutex console_mutex_;
    std::atomic<bool> inline_active_{false};
    
    static thread_local std::string thread_tag_;
    
    std::unique_ptr<std::thread> worker_thread_;
};

// Global convenience functions for performance-critical paths
#define LOG_INFO(tag, message) Logger::instance().info(tag, message)
#define LOG_WARN(tag, message) Logger::instance().warn(tag, message)
#define LOG_ERROR(tag, message) Logger::instance().error(tag, message)

// Trading-specific macros for zero-overhead logging
#define LOG_TRADE_SIGNAL(symbol, signal, price) Logger::instance().log_trade_signal(symbol, signal, price)
#define LOG_TRADE_EXEC(order_id, side, qty, price) Logger::instance().log_trade_execution(order_id, side, qty, price)
#define LOG_ACCOUNT(equity, buying_power) Logger::instance().log_account_update(equity, buying_power)
#define LOG_MARKET_DATA(symbol, price, volume) Logger::instance().log_market_data(symbol, price, volume)

#endif // LOGGER_HPP
