#ifndef SYSTEM_THREADS_HPP
#define SYSTEM_THREADS_HPP

#include <thread>
#include <atomic>
#include <chrono>

/**
 * @brief System thread handles and performance monitoring
 * 
 * Contains thread handles and iteration counters. Implements move semantics.
 */
struct SystemThreads {
    // =========================================================================
    // THREAD HANDLES
    // =========================================================================
    std::thread market_thread;    // Market data processing thread
    std::thread account_thread;   // Account data processing thread
    std::thread gate_thread;      // Market gate control thread
    std::thread trader_thread;    // Main trading logic thread
    std::thread logger_thread;    // Logging system thread
    
    // =========================================================================
    // PERFORMANCE MONITORING
    // =========================================================================
    std::chrono::steady_clock::time_point start_time;  // System startup timestamp
    
    /// Thread iteration counters for performance monitoring
    std::atomic<unsigned long> market_iterations{0};   // Market thread iteration count
    std::atomic<unsigned long> account_iterations{0};  // Account thread iteration count
    std::atomic<unsigned long> gate_iterations{0};     // Gate thread iteration count
    std::atomic<unsigned long> trader_iterations{0};   // Trader thread iteration count
    std::atomic<unsigned long> logger_iterations{0};   // Logger thread iteration count
    
    // =========================================================================
    // CONSTRUCTORS AND ASSIGNMENT
    // =========================================================================
    
    /**
     * @brief Default constructor
     */
    SystemThreads() : start_time(std::chrono::steady_clock::now()) {}
    
    // Copy operations explicitly deleted
    SystemThreads(const SystemThreads&) = delete;
    SystemThreads& operator=(const SystemThreads&) = delete;
    
    /**
     * @brief Move constructor
     * @param other Source object
     */
    SystemThreads(SystemThreads&& other) noexcept
        : market_thread(std::move(other.market_thread)),
          account_thread(std::move(other.account_thread)),
          gate_thread(std::move(other.gate_thread)),
          trader_thread(std::move(other.trader_thread)),
          logger_thread(std::move(other.logger_thread)),
          start_time(other.start_time),
          market_iterations(other.market_iterations.load()),
          account_iterations(other.account_iterations.load()),
          gate_iterations(other.gate_iterations.load()),
          trader_iterations(other.trader_iterations.load()),
          logger_iterations(other.logger_iterations.load()) {}
    
    /**
     * @brief Move assignment operator
     * @param other Source object
     * @return Reference to this object
     */
    SystemThreads& operator=(SystemThreads&& other) noexcept {
        if (this != &other) {
            market_thread = std::move(other.market_thread);
            account_thread = std::move(other.account_thread);
            gate_thread = std::move(other.gate_thread);
            trader_thread = std::move(other.trader_thread);
            logger_thread = std::move(other.logger_thread);
            start_time = other.start_time;
            market_iterations.store(other.market_iterations.load());
            account_iterations.store(other.account_iterations.load());
            gate_iterations.store(other.gate_iterations.load());
            trader_iterations.store(other.trader_iterations.load());
            logger_iterations.store(other.logger_iterations.load());
        }
        return *this;
    }
};

#endif // SYSTEM_THREADS_HPP
