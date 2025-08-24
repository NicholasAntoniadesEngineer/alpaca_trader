#ifndef THREAD_UTILS_HPP
#define THREAD_UTILS_HPP

#include <thread>
#include <string>

// Thread priority levels (cross-platform abstraction)
enum class ThreadPriority {
    LOWEST = 0,
    LOW = 1,
    NORMAL = 2,
    HIGH = 3,
    HIGHEST = 4,
    REALTIME = 5
};

// Thread types for our trading system
enum class ThreadType {
    MAIN,
    TRADER_DECISION,    // Highest priority - critical trading decisions
    MARKET_DATA,        // High priority - real-time market data
    ACCOUNT_DATA,       // Normal priority - account updates
    MARKET_GATE,        // Low priority - market hours checking
    LOGGING             // Lowest priority - background logging
};

// Thread configuration structure
struct ThreadConfig {
    ThreadPriority priority;
    int cpu_affinity;  // -1 for no affinity, 0+ for specific CPU
    std::string name;
    
    ThreadConfig(ThreadPriority prio = ThreadPriority::NORMAL, 
                 int affinity = -1, 
                 const std::string& thread_name = "")
        : priority(prio), cpu_affinity(affinity), name(thread_name) {}
};

// Cross-platform thread utilities
class ThreadUtils {
public:
    // Set thread priority and optional CPU affinity
    static bool set_thread_priority(std::thread& thread, const ThreadConfig& config);
    
    // Set current thread priority
    static bool set_current_thread_priority(const ThreadConfig& config);
    
    // Set thread priority with fallback scaling (tries lower priorities if higher ones fail)
    static ThreadPriority set_thread_priority_with_fallback(std::thread& thread, const ThreadConfig& config);
    
    // Set current thread priority with fallback scaling
    static ThreadPriority set_current_thread_priority_with_fallback(const ThreadConfig& config);
    
    // Get default configuration for thread type
    static ThreadConfig get_default_config(ThreadType type);
    
    // Get current thread info for debugging
    static std::string get_thread_info();
    
    // Set thread name for debugging (Linux/macOS)
    static void set_thread_name(const std::string& name);
    
    // Convert ThreadPriority enum to readable string
    static std::string priority_to_string(ThreadPriority priority);
    
private:
    static int priority_to_native(ThreadPriority priority);
    static bool set_cpu_affinity(std::thread& thread, int cpu);
    static bool set_current_cpu_affinity(int cpu);
};

// RAII thread priority setter for current thread
class ScopedThreadPriority {
private:
    ThreadConfig original_config;
    bool changed;
    
public:
    explicit ScopedThreadPriority(const ThreadConfig& config);
    ~ScopedThreadPriority();
};

#endif // THREAD_UTILS_HPP
