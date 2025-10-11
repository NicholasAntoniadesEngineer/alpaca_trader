#ifndef LOGGING_MACROS_HPP
#define LOGGING_MACROS_HPP

#include "async_logger.hpp"
#include <iostream>

// Standard indentation levels
#define LOG_INDENT_L0 ""              // No indentation
#define LOG_INDENT_L1 "        "      // 8 spaces - Main section level
#define LOG_INDENT_L2 "        "      // 8 spaces + |   - Content level

// Section headers and footers
#define LOG_SECTION_HEADER(title) log_message(LOG_INDENT_L1 "+-- " title, "")

// Content logging macros
#define LOG_CONTENT(msg) log_message(LOG_INDENT_L2 + std::string(msg), "")

// Specialized macros for common patterns
#define LOG_SIGNAL_ANALYSIS_COMPLETE() LOG_SECTION_HEADER("SIGNAL ANALYSIS COMPLETE")

// Trading loop header (special case - no indentation)
#define LOG_TRADING_LOOP_HEADER(loop_num, symbol) \
    log_message("", ""); \
    log_message("================================================================================", ""); \
    log_message("                              TRADING LOOP #" + std::to_string(loop_num) + " - " + symbol, ""); \
    log_message("================================================================================", ""); \
    log_message("", "")

// Thread monitor header (special case - no indentation)
#define LOG_THREAD_MONITOR_HEADER(monitor_num, total_threads, active_threads) \
    log_message("", ""); \
    log_message("================================================================================", ""); \
    log_message("                           THREAD MONITOR #" + std::to_string(monitor_num) + " - " + std::to_string(active_threads) + "/" + std::to_string(total_threads) + " ACTIVE", ""); \
    log_message("================================================================================", ""); \
    log_message("", "")

#define LOG_MESSAGE_BAR() \
    log_message("================================================================================", ""); \
    log_message("", "")

// Thread status section macros
#define LOG_THREAD_STATUS_SECTION() LOG_SECTION_HEADER("THREAD STATUS")
#define LOG_THREAD_PERFORMANCE_SECTION() LOG_SECTION_HEADER("THREAD PERFORMANCE")
#define LOG_THREAD_HEALTH_SECTION() LOG_SECTION_HEADER("THREAD HEALTH")

// Individual thread status macros
#define LOG_THREAD_STATUS(thread_name, status, iterations, cpu_usage) \
    log_message("|   " + std::string(thread_name) + ": " + std::string(status) + " | Iterations: " + std::to_string(iterations) + " | CPU: " + std::to_string(cpu_usage) + "%", "")

#define LOG_THREAD_HEALTH(thread_name, healthy, details) \
    log_message("|   " + std::string(thread_name) + ": " + (healthy ? "HEALTHY" : "ERROR") + (details.empty() ? "" : " - " + details), "")

// Market status message (special case - no indentation)  
#define LOG_MARKET_STATUS(msg) log_message(msg, "")

// Order execution section macros
#define LOG_ORDER_EXECUTION_HEADER() LOG_SECTION_HEADER("ORDER EXECUTION")
#define LOG_DATA_SOURCE_INFO(msg) LOG_CONTENT("DATA SOURCE: " + std::string(msg))
#define LOG_EXIT_TARGETS(msg) LOG_CONTENT("EXIT TARGETS: " + std::string(msg))
#define LOG_ORDER_RESULT(msg) LOG_CONTENT("ORDER RESULT: " + std::string(msg))

// Thread-agnostic macros
#define LOG_THREAD_SECTION_HEADER(title) log_message("+-- " title, "")
#define LOG_THREAD_CONTENT(msg) log_message("" + std::string(msg), "")
#define LOG_THREAD_SUBCONTENT(msg) log_message("" + std::string(msg), "")
#define LOG_THREAD_SEPARATOR() log_message("|", "")
#define LOG_THREAD_SECTION_FOOTER() log_message("+-- ", "")

// Specialized thread macros for common patterns
#define LOG_THREAD_TRADING_CONDITIONS_HEADER() LOG_THREAD_SECTION_HEADER("TRADING CONDITIONS")
#define LOG_THREAD_SIGNAL_ANALYSIS_HEADER(symbol) LOG_THREAD_SECTION_HEADER("SIGNAL ANALYSIS - " + symbol + " (per-lap decisions)")
#define LOG_THREAD_POSITION_SIZING_HEADER() LOG_THREAD_SECTION_HEADER("POSITION SIZING")
#define LOG_THREAD_CURRENT_POSITION_HEADER() LOG_THREAD_SECTION_HEADER("CURRENT POSITION")
#define LOG_THREAD_ORDER_EXECUTION_HEADER() LOG_THREAD_SECTION_HEADER("ORDER EXECUTION")

// Inline status macros for terminal display (not saved to log files)
#define LOG_INLINE_HALT_STATUS(seconds) log_inline_status(AlpacaTrader::Logging::get_formatted_inline_message("|   TRADING HALTED - Next check in " + std::to_string(seconds) + "s"))
#define LOG_INLINE_NEXT_LOOP(seconds) log_inline_status("   ⏳ Next loop in " + std::to_string(seconds) + "s   ")


// Comprehensive table formatting macros for structured logging
#define TABLE_HEADER_48(title, subtitle) do { \
    LOG_THREAD_CONTENT("┌───────────────────┬──────────────────────────────────────────────────┐"); \
    LOG_THREAD_CONTENT("│ " + std::string(title).substr(0,17) + std::string(17 - std::min(17, (int)std::string(title).length()), ' ') + " │ " + std::string(subtitle).substr(0,48) + std::string(48 - std::min(48, (int)std::string(subtitle).length()), ' ') + " │"); \
    LOG_THREAD_CONTENT("├───────────────────┼──────────────────────────────────────────────────┤"); \
} while(0)

#define TABLE_HEADER_30(title, subtitle) do { \
    LOG_THREAD_CONTENT("┌───────────────────┬────────────────────────────────┐"); \
    LOG_THREAD_CONTENT("│ " + std::string(title).substr(0,17) + std::string(17 - std::min(17, (int)std::string(title).length()), ' ') + " │ " + std::string(subtitle).substr(0,30) + std::string(30 - std::min(30, (int)std::string(subtitle).length()), ' ') + " │"); \
    LOG_THREAD_CONTENT("├───────────────────┼────────────────────────────────┤"); \
} while(0)

#define TABLE_ROW_48(label, value) do { \
    std::string label_str = std::string(label).substr(0,17); \
    std::string value_str = std::string(value).substr(0,48); \
    LOG_THREAD_CONTENT("│ " + label_str + std::string(17 - label_str.length(), ' ') + " │ " + value_str + std::string(48 - value_str.length(), ' ') + " │"); \
} while(0)

#define TABLE_ROW_30(label, value) do { \
    std::string label_str = std::string(label).substr(0,17); \
    std::string value_str = std::string(value).substr(0,30); \
    LOG_THREAD_CONTENT("│ " + label_str + std::string(17 - label_str.length(), ' ') + " │ " + value_str + std::string(30 - value_str.length(), ' ') + " │"); \
} while(0)

#define TABLE_SEPARATOR_48() do { \
    LOG_THREAD_CONTENT("├───────────────────┼──────────────────────────────────────────────────┤"); \
} while(0)

#define TABLE_SEPARATOR_30() do { \
    LOG_THREAD_CONTENT("├───────────────────┼────────────────────────────────┤"); \
} while(0)

#define TABLE_FOOTER_48() do { \
    LOG_THREAD_CONTENT("└───────────────────┴──────────────────────────────────────────────────┘"); \
} while(0)

#define TABLE_FOOTER_30() do { \
    LOG_THREAD_CONTENT("└───────────────────┴────────────────────────────────┘"); \
} while(0)

#endif // LOGGING_MACROS_HPP
