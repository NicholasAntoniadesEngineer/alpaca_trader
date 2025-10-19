#ifndef TRADING_MODE_CONFIG_HPP
#define TRADING_MODE_CONFIG_HPP

#include <string>

namespace AlpacaTrader {
namespace Config {

enum class TradingMode {
    STOCKS,
    CRYPTO
};

struct TradingModeConfig {
    TradingMode mode;
    std::string primary_symbol;
    
    static TradingMode parse_mode(const std::string& mode_str) {
        if (mode_str == "stocks" || mode_str == "STOCKS") {
            return TradingMode::STOCKS;
        } else if (mode_str == "crypto" || mode_str == "CRYPTO") {
            return TradingMode::CRYPTO;
        } else {
            throw std::runtime_error("Invalid trading mode: " + mode_str + ". Must be 'stocks' or 'crypto'");
        }
    }
    
    static std::string mode_to_string(TradingMode mode) {
        switch (mode) {
            case TradingMode::STOCKS:
                return "stocks";
            case TradingMode::CRYPTO:
                return "crypto";
            default:
                throw std::runtime_error("Unknown trading mode");
        }
    }
    
    bool is_crypto() const {
        return mode == TradingMode::CRYPTO;
    }
    
    bool is_stocks() const {
        return mode == TradingMode::STOCKS;
    }
};

} // namespace Config
} // namespace AlpacaTrader

#endif // TRADING_MODE_CONFIG_HPP
