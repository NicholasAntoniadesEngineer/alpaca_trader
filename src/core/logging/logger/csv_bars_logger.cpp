#include "csv_bars_logger.hpp"
#include "core/utils/time_utils.hpp"
#include <iostream>
#include "core/logging/logger/async_logger.hpp"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <filesystem>

namespace AlpacaTrader {
namespace Logging {

CSVBarsLogger::CSVBarsLogger(const std::string& log_file_path) : file_path(log_file_path) {
    try {
        // Ensure directory structure exists (parent directory of the file)
        std::filesystem::path file_path_obj(file_path);
        std::filesystem::create_directories(file_path_obj.parent_path());

        file_stream.open(file_path, std::ios::out | std::ios::app);
        if (!file_stream.is_open()) {
            throw std::runtime_error("Failed to open bars log file: " + file_path);
        }

        initialized = true;
        write_header();
    } catch (const std::exception& e) {
        AlpacaTrader::Logging::log_message(std::string("CRITICAL ERROR: Failed to initialize CSV bars logger: ") + e.what(), "");
        throw; // Re-throw to ensure system fails - no defaults allowed
    } catch (...) {
        AlpacaTrader::Logging::log_message("CRITICAL ERROR: Unknown error initializing CSV bars logger", "");
        throw std::runtime_error("Failed to initialize CSV bars logger");
    }
}

CSVBarsLogger::~CSVBarsLogger() {
    if (file_stream.is_open()) {
        file_stream.close();
    }
}

CSVBarsLogger::CSVBarsLogger(CSVBarsLogger&& other) noexcept
    : file_path(std::move(other.file_path)), file_stream(std::move(other.file_stream)), initialized(other.initialized) {
    other.initialized = false;
}

CSVBarsLogger& CSVBarsLogger::operator=(CSVBarsLogger&& other) noexcept {
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

void CSVBarsLogger::write_header() {
    if (!file_stream.is_open()) {
        throw std::runtime_error("Cannot write header - file not open");
    }

    file_stream << "Timestamp,Symbol,Open,High,Low,Close,Volume,ATR,AvgATR,AvgVolume\n";
    file_stream.flush();
}

void CSVBarsLogger::ensure_initialized() {
    if (!initialized || !file_stream.is_open()) {
        throw std::runtime_error("CSV bars logger not properly initialized");
    }
}

void CSVBarsLogger::log_bar(const std::string& timestamp, const std::string& symbol,
                           const AlpacaTrader::Core::Bar& bar, double atr, double avg_atr, double avg_vol) {
    ensure_initialized();

    std::lock_guard<std::mutex> lock(file_mutex);

    file_stream << timestamp << ","
                << symbol << ","
                << std::fixed << std::setprecision(2) << bar.open_price << ","
                << std::fixed << std::setprecision(2) << bar.high_price << ","
                << std::fixed << std::setprecision(2) << bar.low_price << ","
                << std::fixed << std::setprecision(2) << bar.close_price << ","
                << std::fixed << std::setprecision(0) << bar.volume << ","
                << std::fixed << std::setprecision(4) << atr << ","
                << std::fixed << std::setprecision(4) << avg_atr << ","
                << std::fixed << std::setprecision(0) << avg_vol << "\n";

    file_stream.flush();
}

void CSVBarsLogger::log_market_snapshot(const std::string& timestamp, const std::string& symbol,
                                       const AlpacaTrader::Core::MarketSnapshot& snapshot) {
    ensure_initialized();

    std::lock_guard<std::mutex> lock(file_mutex);

    file_stream << timestamp << ","
                << symbol << ","
                << std::fixed << std::setprecision(2) << snapshot.curr.open_price << ","
                << std::fixed << std::setprecision(2) << snapshot.curr.high_price << ","
                << std::fixed << std::setprecision(2) << snapshot.curr.low_price << ","
                << std::fixed << std::setprecision(2) << snapshot.curr.close_price << ","
                << std::fixed << std::setprecision(0) << snapshot.curr.volume << ","
                << std::fixed << std::setprecision(4) << snapshot.atr << ","
                << std::fixed << std::setprecision(4) << snapshot.avg_atr << ","
                << std::fixed << std::setprecision(0) << snapshot.avg_vol << "\n";

    file_stream.flush();
}

void CSVBarsLogger::log_indicators(const std::string& timestamp, const std::string& symbol,
                                  double atr, double avg_atr, double avg_vol, 
                                  double price_change, double volume_change) {
    ensure_initialized();

    std::lock_guard<std::mutex> lock(file_mutex);

    file_stream << timestamp << ","
                << symbol << ","
                << "INDICATORS" << ","
                << std::fixed << std::setprecision(4) << atr << ","
                << std::fixed << std::setprecision(4) << avg_atr << ","
                << std::fixed << std::setprecision(0) << avg_vol << ","
                << std::fixed << std::setprecision(4) << price_change << ","
                << std::fixed << std::setprecision(4) << volume_change << "\n";

    file_stream.flush();
}

void CSVBarsLogger::log_market_data(const std::string& timestamp, const std::string& symbol,
                                   double open, double high, double low, double close, double volume,
                                   double atr, double avg_atr, double avg_vol) {
    ensure_initialized();

    std::lock_guard<std::mutex> lock(file_mutex);

    file_stream << timestamp << ","
                << symbol << ","
                << std::fixed << std::setprecision(2) << open << ","
                << std::fixed << std::setprecision(2) << high << ","
                << std::fixed << std::setprecision(2) << low << ","
                << std::fixed << std::setprecision(2) << close << ","
                << std::fixed << std::setprecision(0) << volume << ","
                << std::fixed << std::setprecision(4) << atr << ","
                << std::fixed << std::setprecision(4) << avg_atr << ","
                << std::fixed << std::setprecision(0) << avg_vol << "\n";

    file_stream.flush();
}

void CSVBarsLogger::flush() {
    if (file_stream.is_open()) {
        file_stream.flush();
    }
}

bool CSVBarsLogger::is_initialized() const {
    return initialized && file_stream.is_open();
}

} // namespace Logging
} // namespace AlpacaTrader
