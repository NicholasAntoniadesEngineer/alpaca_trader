#ifndef CSV_BARS_LOGGER_HPP
#define CSV_BARS_LOGGER_HPP

#include "configs/system_config.hpp"
#include "trader/data_structures/data_structures.hpp"
#include <fstream>
#include <string>
#include <mutex>
#include <memory>

namespace AlpacaTrader {
namespace Logging {

/**
 * CSV logger for market data bars.
 * Logs all bar data in CSV format to runtime_logs/timestamp_logs/bars_logs_*.txt
 */
class CSVBarsLogger {
private:
    std::string file_path;
    std::ofstream file_stream;
    std::mutex file_mutex;
    bool initialized = false;

    void write_header();
    void ensure_initialized();

public:
    explicit CSVBarsLogger(const std::string& log_file_path);
    ~CSVBarsLogger();

    // No default constructor - system must fail if not properly initialized
    CSVBarsLogger() = delete;

    // No copy constructor or assignment
    CSVBarsLogger(const CSVBarsLogger&) = delete;
    CSVBarsLogger& operator=(const CSVBarsLogger&) = delete;

    // Move constructor and assignment for RAII
    CSVBarsLogger(CSVBarsLogger&& other) noexcept;
    CSVBarsLogger& operator=(CSVBarsLogger&& other) noexcept;

    /**
     * Log market bar data
     */
    void log_bar(const std::string& timestamp, const std::string& symbol,
                 const Core::Bar& bar, double atr, double avg_atr, double avg_vol);

    /**
     * Log market snapshot data
     */
    void log_market_snapshot(const std::string& timestamp, const std::string& symbol,
                            const Core::MarketSnapshot& snapshot);

    /**
     * Log technical indicators
     */
    void log_indicators(const std::string& timestamp, const std::string& symbol,
                       double atr, double avg_atr, double avg_vol, 
                       double price_change, double volume_change);

    /**
     * Log market data (OHLCV + indicators)
     */
    void log_market_data(const std::string& timestamp, const std::string& symbol,
                        double open, double high, double low, double close, double volume,
                        double atr, double avg_atr, double avg_vol);

    /**
     * Flush pending data to file
     */
    void flush();

    /**
     * Check if logger is properly initialized
     */
    bool is_initialized() const;
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // CSV_BARS_LOGGER_HPP
