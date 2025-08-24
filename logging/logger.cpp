#include "logger.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <thread>

thread_local std::string Logger::thread_tag_ = "MAIN  ";

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::init(const std::string& log_file_path) {
    if (initialized_.load()) return;
    
    file_path_ = log_file_path;
    running_.store(true);
    worker_thread_ = std::make_unique<std::thread>(&Logger::worker_thread, this);
    initialized_.store(true);
}

void Logger::shutdown() {
    if (!running_.load()) return;
    
    running_.store(false);
    queue_cv_.notify_all();
    
    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }
    
    initialized_.store(false);
}

void Logger::log(Level level, const std::string& tag, const std::string& message) {
    if (!initialized_.load()) {
        // Fallback to console if not initialized
        std::lock_guard<std::mutex> lock(console_mutex_);
        std::cout << "[" << tag << "] " << message << std::endl;
        return;
    }
    
    enqueue_entry(level, tag, message);
}

void Logger::info(const std::string& tag, const std::string& message) {
    log(Level::INFO, tag, message);
}

void Logger::warn(const std::string& tag, const std::string& message) {
    log(Level::WARN, tag, message);
}

void Logger::error(const std::string& tag, const std::string& message) {
    log(Level::ERROR, tag, message);
}

void Logger::log_trade_signal(const std::string& symbol, const std::string& signal, double price) {
    std::ostringstream oss;
    oss << "SIGNAL: " << symbol << " " << signal << " @ $" << std::fixed << std::setprecision(2) << price;
    info("SIGNAL", oss.str());
}

void Logger::log_trade_execution(const std::string& order_id, const std::string& side, int qty, double price) {
    std::ostringstream oss;
    oss << "EXEC: " << order_id << " " << side << " " << qty << " @ $" << std::fixed << std::setprecision(2) << price;
    info("EXEC", oss.str());
}

void Logger::log_account_update(double equity, double buying_power) {
    std::ostringstream oss;
    oss << "ACCOUNT: Equity=$" << std::fixed << std::setprecision(2) << equity 
        << " BP=$" << buying_power;
    info("ACCOUNT", oss.str());
}

void Logger::log_market_data(const std::string& symbol, double price, double volume) {
    std::ostringstream oss;
    oss << "MARKET: " << symbol << " $" << std::fixed << std::setprecision(2) << price 
        << " Vol=" << std::fixed << std::setprecision(0) << volume;
    info("MARKET", oss.str());
}

void Logger::log_thread_performance(const std::string& thread_name, const std::string& metrics) {
    std::ostringstream oss;
    oss << "PERF: " << thread_name << " " << metrics;
    info("PERF", oss.str());
}

void Logger::log_system_status(const std::string& status, const std::string& details) {
    std::string message = status;
    if (!details.empty()) {
        message += " - " + details;
    }
    info("SYSTEM", message);
}

void Logger::set_inline_status(const std::string& message) {
    std::lock_guard<std::mutex> lock(console_mutex_);
    std::cout << "\r" << message << std::flush;
    inline_active_.store(true);
}

void Logger::clear_inline_status() {
    std::lock_guard<std::mutex> lock(console_mutex_);
    if (inline_active_.load()) {
        std::cout << "\r" << std::string(80, ' ') << "\r" << std::flush;
        inline_active_.store(false);
    }
}

void Logger::set_thread_tag(const std::string& tag) {
    std::string formatted_tag = tag;
    if (formatted_tag.size() < 6) {
        formatted_tag.append(6 - formatted_tag.size(), ' ');
    } else if (formatted_tag.size() > 6) {
        formatted_tag = formatted_tag.substr(0, 6);
    }
    thread_tag_ = formatted_tag;
}

std::string Logger::get_thread_info() const {
    std::ostringstream oss;
    oss << "TID:" << std::this_thread::get_id();
    return oss.str();
}

void Logger::worker_thread() {
    std::ofstream log_file(file_path_, std::ios::app);
    
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] { return !log_queue_.empty() || !running_.load(); });
        
        while (!log_queue_.empty()) {
            LogEntry entry = std::move(log_queue_.front());
            log_queue_.pop();
            lock.unlock();
            
            // Output to console
            {
                std::lock_guard<std::mutex> console_lock(console_mutex_);
                if (inline_active_.load()) {
                    std::cout << std::endl;
                    inline_active_.store(false);
                }
                std::cout << entry.formatted_line;
            }
            
            // Output to file
            if (log_file.is_open()) {
                log_file << entry.formatted_line;
                log_file.flush();
            }
            
            lock.lock();
        }
    }
}

void Logger::enqueue_entry(Level level, const std::string& tag, const std::string& message) {
    LogEntry entry;
    entry.level = level;
    entry.timestamp = format_timestamp();
    entry.tag = tag;
    entry.message = message;
    entry.formatted_line = format_entry(entry);
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        log_queue_.push(std::move(entry));
    }
    queue_cv_.notify_one();
}

std::string Logger::format_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto local_tm = *std::localtime(&time_t);
    
    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string Logger::format_entry(const LogEntry& entry) const {
    std::ostringstream oss;
    oss << entry.timestamp << " [" << thread_tag_ << "]   " << entry.message << std::endl;
    return oss.str();
}
