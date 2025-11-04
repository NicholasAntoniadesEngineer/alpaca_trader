#include "multi_api_config_loader.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace AlpacaTrader {
namespace Core {

Config::MultiApiConfig MultiApiConfigLoader::load_from_csv(const std::string& csv_path) {
    if (csv_path.empty()) {
        throw std::runtime_error("CSV path is required but not provided");
    }
    
    std::ifstream file(csv_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open API configuration file: " + csv_path);
    }
    
    Config::MultiApiConfig config;
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        std::stringstream ss(line);
        std::string key, value;
        
        if (!std::getline(ss, key, ',')) {
            continue;
        }
        
        if (!std::getline(ss, value)) {
            continue;
        }
        
        key = trim(key);
        value = trim(value);
        
        if (key.empty()) {
            continue;
        }
        
        parse_provider_config(key, value, config);
    }
    
    validate_required_fields(config);
    
    return config;
}

void MultiApiConfigLoader::parse_provider_config(const std::string& key, const std::string& value, 
                                                Config::MultiApiConfig& config) {
    if (key.find('.') == std::string::npos) {
        return;
    }
    
    std::string provider_str = key.substr(0, key.find('.'));
    std::string field = key.substr(key.find('.') + 1);
    
    Config::ApiProvider provider;
    try {
        provider = get_provider_from_key(provider_str);
    } catch (const std::exception&) {
        return;
    }
    
    if (config.providers.find(provider) == config.providers.end()) {
        config.providers[provider] = Config::ApiProviderConfig{};
    }
    
    Config::ApiProviderConfig& provider_config = config.providers[provider];
    
    if (field == "api_key") {
        if (value.empty() || value == "YOUR_POLYGON_API_KEY_HERE") {
            throw std::runtime_error("API key is required for provider: " + provider_str);
        }
        provider_config.api_key = value;
    } else if (field == "api_secret") {
        provider_config.api_secret = value;
    } else if (field == "base_url") {
        if (value.empty()) {
            throw std::runtime_error("Base URL is required for provider: " + provider_str);
        }
        provider_config.base_url = value;
    } else if (field == "websocket_url") {
        provider_config.websocket_url = value;
    } else if (field == "retry_count") {
        if (value.empty()) {
            throw std::runtime_error("Retry count is required for provider: " + provider_str);
        }
        provider_config.retry_count = std::stoi(value);
    } else if (field == "timeout_seconds") {
        if (value.empty()) {
            throw std::runtime_error("Timeout seconds is required for provider: " + provider_str);
        }
        provider_config.timeout_seconds = std::stoi(value);
    } else if (field == "enable_ssl_verification") {
        if (value.empty()) {
            throw std::runtime_error("SSL verification setting is required for provider: " + provider_str);
        }
        provider_config.enable_ssl_verification = to_bool(value);
    } else if (field == "rate_limit_delay_ms") {
        if (value.empty()) {
            throw std::runtime_error("Rate limit delay is required for provider: " + provider_str);
        }
        provider_config.rate_limit_delay_ms = std::stoi(value);
    } else if (field == "api_version") {
        if (value.empty()) {
            throw std::runtime_error("API version is required for provider: " + provider_str);
        }
        provider_config.api_version = value;
    } else if (field == "bar_timespan") {
        provider_config.bar_timespan = value;
    } else if (field == "bar_multiplier") {
        if (!value.empty()) {
            provider_config.bar_multiplier = std::stoi(value);
        }
    } else if (field == "bars_range_minutes") {
        if (value.empty()) {
            throw std::runtime_error("bars_range_minutes is required for provider: " + provider_str);
        }
        provider_config.bars_range_minutes = std::stoi(value);
    } else if (field == "websocket_bar_accumulation_seconds") {
        if (!value.empty()) {
            provider_config.websocket_bar_accumulation_seconds = std::stoi(value);
        }
    } else if (field == "websocket_second_level_accumulation_seconds") {
        if (!value.empty()) {
            provider_config.websocket_second_level_accumulation_seconds = std::stoi(value);
        }
    } else if (field == "websocket_max_bar_history_size") {
        if (!value.empty()) {
            provider_config.websocket_max_bar_history_size = std::stoi(value);
        }
    } else if (field.find("endpoints.") == 0) {
        std::string endpoint_name = field.substr(10);
        
        if (endpoint_name == "bars") {
            provider_config.endpoints.bars = value;
        } else if (endpoint_name == "quotes_latest") {
            provider_config.endpoints.quotes_latest = value;
        } else if (endpoint_name == "trades") {
            provider_config.endpoints.trades = value;
        } else if (endpoint_name == "account") {
            provider_config.endpoints.account = value;
        } else if (endpoint_name == "positions") {
            provider_config.endpoints.positions = value;
        } else if (endpoint_name == "orders") {
            provider_config.endpoints.orders = value;
        } else if (endpoint_name == "clock") {
            provider_config.endpoints.clock = value;
        } else if (endpoint_name == "assets") {
            provider_config.endpoints.assets = value;
        }
    }
}

Config::ApiProvider MultiApiConfigLoader::get_provider_from_key(const std::string& key) {
    if (key == "alpaca_trading") {
        return Config::ApiProvider::ALPACA_TRADING;
    } else if (key == "alpaca_stocks") {
        return Config::ApiProvider::ALPACA_STOCKS;
    } else if (key == "polygon_crypto") {
        return Config::ApiProvider::POLYGON_CRYPTO;
    } else {
        throw std::runtime_error("Unknown API provider: " + key);
    }
}

std::string MultiApiConfigLoader::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

bool MultiApiConfigLoader::to_bool(const std::string& str) {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    
    if (lower_str == "true" || lower_str == "1" || lower_str == "yes") {
        return true;
    } else if (lower_str == "false" || lower_str == "0" || lower_str == "no") {
        return false;
    } else {
        throw std::runtime_error("Invalid boolean value: " + str);
    }
}

void MultiApiConfigLoader::validate_required_fields(const Config::MultiApiConfig& config) {
    if (config.providers.empty()) {
        throw std::runtime_error("No API providers configured");
    }
    
    for (const auto& [provider, provider_config] : config.providers) {
        validate_provider_config(provider, provider_config);
    }
}

void MultiApiConfigLoader::validate_provider_config(Config::ApiProvider provider, 
                                                   const Config::ApiProviderConfig& config) {
    std::string provider_name;
    switch (provider) {
        case Config::ApiProvider::ALPACA_TRADING:
            provider_name = "alpaca_trading";
            break;
        case Config::ApiProvider::ALPACA_STOCKS:
            provider_name = "alpaca_stocks";
            break;
        case Config::ApiProvider::POLYGON_CRYPTO:
            provider_name = "polygon_crypto";
            break;
        default:
            throw std::runtime_error("Unknown API provider type");
    }
    
    if (config.api_key.empty()) {
        throw std::runtime_error("API key is required for provider: " + provider_name);
    }
    
    if (config.base_url.empty()) {
        throw std::runtime_error("Base URL is required for provider: " + provider_name);
    }
    
    if (config.retry_count <= 0) {
        throw std::runtime_error("Retry count must be greater than 0 for provider: " + provider_name);
    }
    
    if (config.timeout_seconds <= 0) {
        throw std::runtime_error("Timeout seconds must be greater than 0 for provider: " + provider_name);
    }
    
    if (config.rate_limit_delay_ms < 0) {
        throw std::runtime_error("Rate limit delay cannot be negative for provider: " + provider_name);
    }
    
    if (config.api_version.empty()) {
        throw std::runtime_error("API version is required for provider: " + provider_name);
    }
    
    if (provider == Config::ApiProvider::ALPACA_TRADING) {
        if (config.endpoints.account.empty()) {
            throw std::runtime_error("Account endpoint is required for Alpaca trading provider");
        }
        if (config.endpoints.positions.empty()) {
            throw std::runtime_error("Positions endpoint is required for Alpaca trading provider");
        }
        if (config.endpoints.orders.empty()) {
            throw std::runtime_error("Orders endpoint is required for Alpaca trading provider");
        }
    }
    
    if (provider == Config::ApiProvider::ALPACA_STOCKS || provider == Config::ApiProvider::POLYGON_CRYPTO) {
        if (config.endpoints.bars.empty()) {
            throw std::runtime_error("Bars endpoint is required for market data provider: " + provider_name);
        }
        if (config.endpoints.quotes_latest.empty()) {
            throw std::runtime_error("Quotes endpoint is required for market data provider: " + provider_name);
        }
        if (config.bar_multiplier <= 0) {
            throw std::runtime_error("bar_multiplier must be > 0 for market data provider: " + provider_name);
        }
        if (config.bar_timespan.empty()) {
            throw std::runtime_error("bar_timespan is required for market data provider: " + provider_name);
        }
        if (config.bars_range_minutes <= 0) {
            throw std::runtime_error("bars_range_minutes must be > 0 for market data provider: " + provider_name);
        }
        
        if (provider == Config::ApiProvider::POLYGON_CRYPTO) {
            if (config.websocket_bar_accumulation_seconds <= 0) {
                throw std::runtime_error("websocket_bar_accumulation_seconds must be configured and > 0 for polygon_crypto provider");
            }
            if (config.websocket_second_level_accumulation_seconds <= 0) {
                throw std::runtime_error("websocket_second_level_accumulation_seconds must be configured and > 0 for polygon_crypto provider");
            }
            if (config.websocket_max_bar_history_size <= 0) {
                throw std::runtime_error("websocket_max_bar_history_size must be configured and > 0 for polygon_crypto provider");
            }
            if (config.websocket_second_level_accumulation_seconds % config.websocket_bar_accumulation_seconds != 0) {
                throw std::runtime_error("websocket_second_level_accumulation_seconds must be a multiple of websocket_bar_accumulation_seconds for polygon_crypto provider");
            }
        }
    }
}

} // namespace Core
} // namespace AlpacaTrader
