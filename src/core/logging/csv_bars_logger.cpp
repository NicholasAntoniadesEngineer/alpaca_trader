#include "csv_bars_logger.hpp"
#include "core/utils/time_utils.hpp"
#include <iostream>
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
        std::cerr << "CRITICAL ERROR: Failed to initialize CSV bars logger: " << e.what() << std::endl;
        throw; // Re-throw to ensure system fails - no defaults allowed
    } catch (...) {
        std::cerr << "CRITICAL ERROR: Unknown error initializing CSV bars logger" << std::endl;
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

void CSVBarsLogger::log_bar(const AlpacaTrader::Core::Bar& bar, const std::string& symbol,
                           const std::string& timestamp, double atr, double avg_atr, double avg_vol) {
    ensure_initialized();

    std::lock_guard<std::mutex> lock(file_mutex);

    file_stream << timestamp << ","
                << symbol << ","
                << std::fixed << std::setprecision(2) << bar.o << ","
                << std::fixed << std::setprecision(2) << bar.h << ","
                << std::fixed << std::setprecision(2) << bar.l << ","
                << std::fixed << std::setprecision(2) << bar.c << ","
                << std::fixed << std::setprecision(0) << bar.v << ","
                << std::fixed << std::setprecision(4) << atr << ","
                << std::fixed << std::setprecision(4) << avg_atr << ","
                << std::fixed << std::setprecision(0) << avg_vol << "\n";

    file_stream.flush();
}

void CSVBarsLogger::log_bars(const std::vector<AlpacaTrader::Core::Bar>& bars, const std::string& symbol,
                            const std::string& timestamp, double atr, double avg_atr, double avg_vol) {
    ensure_initialized();

    std::lock_guard<std::mutex> lock(file_mutex);

    for (const auto& bar : bars) {
        // Write bar data directly without acquiring mutex again
        file_stream << timestamp << ","
                    << symbol << ","
                    << std::fixed << std::setprecision(2) << bar.o << ","
                    << std::fixed << std::setprecision(2) << bar.h << ","
                    << std::fixed << std::setprecision(2) << bar.l << ","
                    << std::fixed << std::setprecision(2) << bar.c << ","
                    << std::fixed << std::setprecision(0) << bar.v << ","
                    << std::fixed << std::setprecision(4) << atr << ","
                    << std::fixed << std::setprecision(4) << avg_atr << ","
                    << std::fixed << std::setprecision(0) << avg_vol << "\n";
    }
    
    file_stream.flush();
}

void CSVBarsLogger::log_quote(const AlpacaTrader::Core::QuoteData& quote, const std::string& symbol,
                             const std::string& timestamp, double atr, double avg_atr, double avg_vol) {
    ensure_initialized();

    std::lock_guard<std::mutex> lock(file_mutex);

    // Log quote data in a format similar to bars but with quote-specific fields
    file_stream << timestamp << ","
                << symbol << ","
                << std::fixed << std::setprecision(2) << quote.bid_price << ","  // Use bid as "open"
                << std::fixed << std::setprecision(2) << quote.ask_price << ","  // Use ask as "high"
                << std::fixed << std::setprecision(2) << quote.bid_price << ","  // Use bid as "low"
                << std::fixed << std::setprecision(2) << quote.mid_price << ","  // Use mid as "close"
                << std::fixed << std::setprecision(2) << (quote.ask_size + quote.bid_size) << "," // Use combined size as "volume"
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

} // namespace Logging
} // namespace AlpacaTrader
