#ifndef CSV_TRADE_LOGGER_HPP
#define CSV_TRADE_LOGGER_HPP

#include "configs/system_config.hpp"
#include "core/trader/data/data_structures.hpp"
#include <fstream>
#include <string>
#include <mutex>
#include <memory>

namespace AlpacaTrader {
namespace Logging {

/**
 * CSV logger for trading operations.
 * Logs all trading activities in CSV format to runtime_logs/timestamp_logs/trade_logs_*.txt
 */
class CSVTradeLogger {
private:
    std::string file_path;
    std::ofstream file_stream;
    std::mutex file_mutex;
    bool initialized = false;

    void write_header();
    void ensure_initialized();

public:
    explicit CSVTradeLogger(const std::string& log_file_path);
    ~CSVTradeLogger();

    // No default constructor - system must fail if not properly initialized
    CSVTradeLogger() = delete;

    // No copy constructor or assignment
    CSVTradeLogger(const CSVTradeLogger&) = delete;
    CSVTradeLogger& operator=(const CSVTradeLogger&) = delete;

    // Move constructor and assignment for RAII
    CSVTradeLogger(CSVTradeLogger&& other) noexcept;
    CSVTradeLogger& operator=(CSVTradeLogger&& other) noexcept;

    /**
     * Log a trading signal decision
     */
    void log_signal(const std::string& timestamp, const std::string& symbol,
                    bool buy_signal, bool sell_signal, double signal_strength,
                    const std::string& reason);

    /**
     * Log filter results
     */
    void log_filters(const std::string& timestamp, const std::string& symbol,
                     bool atr_pass, double atr_ratio, double atr_threshold,
                     bool vol_pass, double vol_ratio, double vol_threshold,
                     bool doji_pass);

    /**
     * Log position sizing decision
     */
    void log_position_sizing(const std::string& timestamp, const std::string& symbol,
                             int quantity, double risk_amount, double position_value,
                             double buying_power);

    /**
     * Log order execution
     */
    void log_order_execution(const std::string& timestamp, const std::string& symbol,
                             const std::string& side, int quantity, double price,
                             const std::string& order_type, const std::string& order_id,
                             const std::string& status);

    /**
     * Log position changes
     */
    void log_position_change(const std::string& timestamp, const std::string& symbol,
                             int previous_qty, int current_qty, double unrealized_pnl);

    /**
     * Log account updates
     */
    void log_account_update(const std::string& timestamp, double equity, double buying_power,
                            double exposure_pct);

    /**
     * Log market data summary
     */
    void log_market_data(const std::string& timestamp, const std::string& symbol,
                         double open, double high, double low, double close, double volume,
                         double atr, double avg_atr, double avg_vol);

    /**
     * Flush any buffered data to file
     */
    void flush();

    /**
     * Check if logger is properly initialized and file is open
     */
    bool is_valid() const { return initialized && file_stream.is_open(); }
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // CSV_TRADE_LOGGER_HPP
