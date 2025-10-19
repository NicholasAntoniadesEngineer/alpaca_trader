#include "csv_trade_logger.hpp"
#include "core/utils/time_utils.hpp"
#include <iomanip>
#include <stdexcept>

namespace AlpacaTrader {
namespace Logging {

CSVTradeLogger::CSVTradeLogger(const std::string& log_file_path) : file_path(log_file_path) {
    file_stream.open(file_path, std::ios::out | std::ios::app);
    if (!file_stream.is_open()) {
        throw std::runtime_error("Failed to open CSV trade log file: " + file_path);
    }
    
    // Write header if file is empty
    file_stream.seekp(0, std::ios::end);
    if (file_stream.tellp() == 0) {
        write_header();
    }
    
    initialized = true;
}

CSVTradeLogger::~CSVTradeLogger() {
    if (file_stream.is_open()) {
        file_stream.close();
    }
}

CSVTradeLogger::CSVTradeLogger(CSVTradeLogger&& other) noexcept
    : file_path(std::move(other.file_path)),
      file_stream(std::move(other.file_stream)),
      file_mutex(),
      initialized(other.initialized) {
    other.initialized = false;
}

CSVTradeLogger& CSVTradeLogger::operator=(CSVTradeLogger&& other) noexcept {
    if (this != &other) {
        if (file_stream.is_open()) {
            file_stream.close();
        }
        
        file_path = std::move(other.file_path);
        file_stream = std::move(other.file_stream);
        initialized = other.initialized;
        other.initialized = false;
    }
    return *this;
}

void CSVTradeLogger::write_header() {
    file_stream << "timestamp,symbol,event_type,value1,value2,value3,value4,value5,notes\n";
    file_stream.flush();
}

void CSVTradeLogger::ensure_initialized() {
    if (!initialized || !file_stream.is_open()) {
        throw std::runtime_error("CSV trade logger not properly initialized");
    }
}

void CSVTradeLogger::log_signal(const std::string& timestamp, const std::string& symbol,
                                bool buy_signal, bool sell_signal, double signal_strength,
                                const std::string& reason) {
    ensure_initialized();
    std::lock_guard<std::mutex> lock(file_mutex);

    file_stream << timestamp << ","
                << symbol << ","
                << "SIGNAL" << ","
                << (buy_signal ? "BUY" : (sell_signal ? "SELL" : "NONE")) << ","
                << std::fixed << std::setprecision(4) << signal_strength << ","
                << reason << ",,,," << "\n";

    file_stream.flush();
}

void CSVTradeLogger::log_filters(const std::string& timestamp, const std::string& symbol,
                                 bool atr_pass, double atr_ratio, double atr_threshold,
                                 bool vol_pass, double vol_ratio,
                                 bool doji_pass) {
    ensure_initialized();
    std::lock_guard<std::mutex> lock(file_mutex);

    file_stream << timestamp << ","
                << symbol << ","
                << "FILTERS" << ","
                << (atr_pass ? "PASS" : "FAIL") << ","
                << std::fixed << std::setprecision(4) << atr_ratio << ","
                << std::fixed << std::setprecision(4) << atr_threshold << ","
                << (vol_pass ? "PASS" : "FAIL") << ","
                << std::fixed << std::setprecision(4) << vol_ratio << ","
                << (doji_pass ? "PASS" : "FAIL") << "\n";

    file_stream.flush();
}

void CSVTradeLogger::log_position_sizing(const std::string& timestamp, const std::string& symbol,
                                         int quantity, double risk_amount, double position_value,
                                         double buying_power) {
    ensure_initialized();
    std::lock_guard<std::mutex> lock(file_mutex);

    file_stream << timestamp << ","
                << symbol << ","
                << "POSITION_SIZING" << ","
                << quantity << ","
                << std::fixed << std::setprecision(2) << risk_amount << ","
                << std::fixed << std::setprecision(2) << position_value << ","
                << std::fixed << std::setprecision(2) << buying_power << ",," << "\n";

    file_stream.flush();
}

void CSVTradeLogger::log_order_execution(const std::string& timestamp, const std::string& symbol,
                                         const std::string& side, int quantity, double price,
                                         const std::string& order_type, const std::string& order_id,
                                         const std::string& status) {
    ensure_initialized();
    std::lock_guard<std::mutex> lock(file_mutex);

    file_stream << timestamp << ","
                << symbol << ","
                << "ORDER_EXECUTION" << ","
                << side << ","
                << quantity << ","
                << std::fixed << std::setprecision(2) << price << ","
                << order_type << ","
                << order_id << ","
                << status << "\n";

    file_stream.flush();
}

void CSVTradeLogger::log_position_change(const std::string& timestamp, const std::string& symbol,
                                         int previous_qty, int current_qty, double unrealized_pnl) {
    ensure_initialized();
    std::lock_guard<std::mutex> lock(file_mutex);

    file_stream << timestamp << ","
                << symbol << ","
                << "POSITION_CHANGE" << ","
                << previous_qty << ","
                << current_qty << ","
                << std::fixed << std::setprecision(2) << unrealized_pnl << ",,," << "\n";

    file_stream.flush();
}

void CSVTradeLogger::log_account_update(const std::string& timestamp, double equity, double buying_power,
                                        double exposure_pct) {
    ensure_initialized();
    std::lock_guard<std::mutex> lock(file_mutex);

    file_stream << timestamp << ","
                << "ACCOUNT" << ","
                << "ACCOUNT_UPDATE" << ","
                << std::fixed << std::setprecision(2) << equity << ","
                << std::fixed << std::setprecision(2) << buying_power << ","
                << std::fixed << std::setprecision(4) << exposure_pct << ",,," << "\n";

    file_stream.flush();
}

void CSVTradeLogger::log_market_data(const std::string& timestamp, const std::string& symbol,
                                     double open, double high, double low, double close, double volume,
                                     double atr) {
    ensure_initialized();
    std::lock_guard<std::mutex> lock(file_mutex);

    file_stream << timestamp << ","
                << symbol << ","
                << "MARKET_DATA" << ","
                << std::fixed << std::setprecision(2) << open << ","
                << std::fixed << std::setprecision(2) << high << ","
                << std::fixed << std::setprecision(2) << low << ","
                << std::fixed << std::setprecision(2) << close << ","
                << std::fixed << std::setprecision(0) << volume << ","
                << std::fixed << std::setprecision(4) << atr << "\n";

    file_stream.flush();
}

void CSVTradeLogger::flush() {
    if (file_stream.is_open()) {
        file_stream.flush();
    }
}

// Note: The header declares is_valid() as inline, so no implementation needed here

} // namespace Logging
} // namespace AlpacaTrader
