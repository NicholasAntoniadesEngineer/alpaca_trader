#include "polygon_crypto_client.hpp"
#include "utils/http_utils.hpp"
#include "json/json.hpp"
#include "api/polygon/bar_accumulator.hpp"
#include "logging/logs/websocket_logs.hpp"
#include "logging/logs/market_data_logs.hpp"
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <random>
#include <algorithm>
#include <cstring>

using json = nlohmann::json;

namespace AlpacaTrader {
namespace API {

PolygonCryptoClient::PolygonCryptoClient(ConnectivityManager& connectivity_mgr) 
    : connectivity_manager(connectivity_mgr), lastStaleDataLogTime(std::chrono::steady_clock::now()), lastBarsLogTime(std::chrono::steady_clock::now()) {}

PolygonCryptoClient::~PolygonCryptoClient() {
    disconnect();
}

bool PolygonCryptoClient::initialize(const Config::ApiProviderConfig& configuration) {
    if (!validate_config()) {
        return false;
    }
    
    config = configuration;
    
    if (config.api_key.empty()) {
        throw std::runtime_error("Polygon.io API key is required but not provided");
    }
    
    if (config.base_url.empty()) {
        throw std::runtime_error("Polygon.io base URL is required but not provided");
    }
    
    connected = true;
    return true;
}

bool PolygonCryptoClient::is_connected() const {
    return connected.load();
}

void PolygonCryptoClient::disconnect() {
    connected = false;
    stop_realtime_feed();
    cleanup_resources();
}

std::vector<Core::Bar> PolygonCryptoClient::get_recent_bars(const Core::BarRequest& request) const {
    if (!is_connected()) {
        throw std::runtime_error("Polygon client not connected");
    }
    
    if (request.symbol.empty()) {
        throw std::runtime_error("Symbol is required for bar request");
    }
    
    if (request.limit <= 0) {
        throw std::runtime_error("Limit must be greater than 0 for bar request");
    }

    bool websocketNeedsStartFlag = false;
    {
        std::lock_guard<std::mutex> websocketCheckGuard(data_mutex);
        if (!websocket_active.load()) {
            websocketNeedsStartFlag = true;
        }
    }
    
    if (websocketNeedsStartFlag) {
        std::vector<std::string> symbolVector;
        symbolVector.push_back(request.symbol);
        
        bool startResultValue = const_cast<PolygonCryptoClient*>(this)->start_realtime_feed(symbolVector);
        
        if (!startResultValue) {
            throw std::runtime_error("Failed to start WebSocket real-time feed for symbol: " + request.symbol);
        }
        
        std::lock_guard<std::mutex> websocketVerifyGuard(data_mutex);
        if (!websocket_active.load()) {
            throw std::runtime_error("WebSocket is not active after attempting to start real-time feed");
        }
    }
    
    Polygon::BarAccumulator* accumulatorPointer = nullptr;
    {
        std::lock_guard<std::mutex> dataGuard(data_mutex);
        
        auto accumulatorIterator = barAccumulatorMap.find(request.symbol);
        if (accumulatorIterator != barAccumulatorMap.end() && accumulatorIterator->second) {
            accumulatorPointer = accumulatorIterator->second.get();
    }
    }
    
    if (!accumulatorPointer) {
        std::vector<std::string> symbolVector;
        symbolVector.push_back(request.symbol);
        
        if (!const_cast<PolygonCryptoClient*>(this)->start_realtime_feed(symbolVector)) {
            throw std::runtime_error("Failed to start WebSocket real-time feed for symbol: " + request.symbol);
        }
        
        std::lock_guard<std::mutex> dataGuardRetry(data_mutex);
        auto accumulatorIterator = barAccumulatorMap.find(request.symbol);
        if (accumulatorIterator == barAccumulatorMap.end() || !accumulatorIterator->second) {
            throw std::runtime_error("No accumulator found for symbol: " + request.symbol + " after starting feed");
        }
        accumulatorPointer = accumulatorIterator->second.get();
    }
    
    if (!accumulatorPointer) {
        throw std::runtime_error("No accumulator available for symbol: " + request.symbol);
    }
    
    size_t accumulatorBarCountValue = accumulatorPointer->getAccumulatedBarsCount();
    
    std::vector<Core::Bar> accumulatedBarsResult = accumulatorPointer->getAccumulatedBars(request.limit);
    if (accumulatedBarsResult.empty()) {
        std::string errorMessageString = "WebSocket is active but no accumulated bars available yet. ";
        errorMessageString += "Accumulator has: " + std::to_string(accumulatorBarCountValue) + " bars. ";
        errorMessageString += "Waiting for real-time data to accumulate. ";
        errorMessageString += "First level requires " + std::to_string(config.websocket_bar_accumulation_seconds) + " seconds, ";
        errorMessageString += "second level requires " + std::to_string(config.websocket_second_level_accumulation_seconds) + " seconds of data.";
        throw std::runtime_error(errorMessageString);
    }
    
    if (accumulatedBarsResult.empty()) {
        throw std::runtime_error("No accumulated bars available from WebSocket accumulator");
    }
    
    auto currentTimeMillisValue = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    long long latestBarFreshnessThresholdMillisValue = static_cast<long long>(config.websocket_bar_accumulation_seconds) * 1000LL * 6;
    
    Core::Bar latestBarValue = accumulatedBarsResult.back();
    
    try {
        long long latestBarTimestampMillisValue = std::stoll(latestBarValue.timestamp);
        
        if (latestBarTimestampMillisValue <= 0) {
            std::string invalidTimestampErrorString = "Invalid timestamp in latest WebSocket bar: " + latestBarValue.timestamp + " (must be positive Unix milliseconds)";
            AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details("INVALID_TIMESTAMP", invalidTimestampErrorString, "trading_system.log");
            throw std::runtime_error(invalidTimestampErrorString);
        }
        
        long long latestBarAgeMillisValue = currentTimeMillisValue - latestBarTimestampMillisValue;
        long long latestBarAgeSecondsValue = latestBarAgeMillisValue / 1000;
        
        if (latestBarAgeMillisValue > latestBarFreshnessThresholdMillisValue) {
            auto nowTimeValue = std::chrono::steady_clock::now();
            auto timeSinceLastStaleLogValue = std::chrono::duration_cast<std::chrono::seconds>(nowTimeValue - lastStaleDataLogTime).count();
            bool shouldLogStaleDataFlag = (timeSinceLastStaleLogValue >= 60) || (lastStaleDataTimestamp != latestBarValue.timestamp);
            
            if (shouldLogStaleDataFlag) {
                try {
                    int maxAgeSecondsValue = config.websocket_bar_accumulation_seconds * 6;
                    AlpacaTrader::Logging::WebSocketLogs::log_websocket_stale_data_table(
                        latestBarValue.timestamp,
                        latestBarAgeSecondsValue,
                        maxAgeSecondsValue,
                        "trading_system.log"
                    );
                    lastStaleDataLogTime = nowTimeValue;
                    lastStaleDataTimestamp = latestBarValue.timestamp;
                } catch (...) {
                    // Logging failed, continue
                }
            }
            
            std::string staleLatestBarErrorString = "Latest accumulated bar is stale. ";
            staleLatestBarErrorString += "Latest bar timestamp: " + latestBarValue.timestamp + " (" + std::to_string(latestBarAgeSecondsValue) + " seconds old). ";
            staleLatestBarErrorString += "Maximum allowed age for latest bar: " + std::to_string(config.websocket_bar_accumulation_seconds * 6) + " seconds. ";
            staleLatestBarErrorString += "WebSocket must provide recent data. Total accumulated bars: " + std::to_string(accumulatedBarsResult.size()) + ".";
            throw std::runtime_error(staleLatestBarErrorString);
        }
        
    } catch (const std::runtime_error& runtimeExceptionError) {
        throw;
    } catch (const std::exception& generalExceptionError) {
        std::string invalidTimestampErrorString = "Invalid timestamp in WebSocket bar data: " + latestBarValue.timestamp + ". Error: " + std::string(generalExceptionError.what());
        AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details("INVALID_TIMESTAMP", invalidTimestampErrorString, "trading_system.log");
        throw std::runtime_error(invalidTimestampErrorString);
    } catch (...) {
        throw std::runtime_error("Unknown error validating WebSocket bar timestamps");
    }
    
    if (static_cast<int>(accumulatedBarsResult.size()) < request.limit) {
        std::string insufficientBarsErrorString = "Insufficient accumulated bars available. ";
        insufficientBarsErrorString += "Have " + std::to_string(accumulatedBarsResult.size()) + " bars, need " + std::to_string(request.limit) + ". ";
        insufficientBarsErrorString += "Waiting for more real-time data to accumulate.";
        throw std::runtime_error(insufficientBarsErrorString);
    }
    
    bool shouldLogBarsFlag = false;
    {
        std::lock_guard<std::mutex> logCheckGuard(data_mutex);
        
        auto nowTimeValue = std::chrono::steady_clock::now();
        auto timeSinceLastBarsLogValue = std::chrono::duration_cast<std::chrono::seconds>(nowTimeValue - lastBarsLogTime).count();
        
        auto lastLoggedBarsIterator = lastLoggedBarsMap.find(request.symbol);
        if (lastLoggedBarsIterator == lastLoggedBarsMap.end()) {
            shouldLogBarsFlag = true;
        } else {
            const std::vector<Core::Bar>& lastLoggedBarsValue = lastLoggedBarsIterator->second;
            if (accumulatedBarsResult.size() != lastLoggedBarsValue.size()) {
                shouldLogBarsFlag = true;
            } else if (!accumulatedBarsResult.empty() && !lastLoggedBarsValue.empty()) {
                const Core::Bar& currentLatestBar = accumulatedBarsResult.back();
                const Core::Bar& lastLoggedLatestBar = lastLoggedBarsValue.back();
                if (currentLatestBar.timestamp != lastLoggedLatestBar.timestamp ||
                    currentLatestBar.close_price != lastLoggedLatestBar.close_price ||
                    currentLatestBar.volume != lastLoggedLatestBar.volume) {
                    shouldLogBarsFlag = true;
        }
            }
            
            if (!shouldLogBarsFlag && timeSinceLastBarsLogValue >= 60) {
                shouldLogBarsFlag = true;
            }
        }
        
        if (shouldLogBarsFlag) {
            lastLoggedBarsMap[request.symbol] = accumulatedBarsResult;
            lastBarsLogTime = std::chrono::steady_clock::now();
        }
    }
    
    if (shouldLogBarsFlag) {
        try {
            // Compliance: Validate request.limit before passing (must be > 0 per config validation)
            if (request.limit <= 0) {
                throw std::runtime_error("Invalid bars_required for logging: request.limit must be > 0. Got: " + std::to_string(request.limit));
            }
            // Pass the requested limit (bars_to_fetch_for_calculations) to show progress
            AlpacaTrader::Logging::MarketDataLogs::log_all_bars_received(request.symbol, accumulatedBarsResult, "trading_system.log", request.limit);
        } catch (const std::exception& logging_exception_error) {
            // Log error but continue - logging failure should not stop data flow
            try {
                AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                    "LOGGING_ERROR",
                    "Failed to log accumulated bars: " + std::string(logging_exception_error.what()),
                    "trading_system.log"
                );
            } catch (...) {
                // Logging of logging error failed - continue anyway
            }
        } catch (...) {
            // Unknown exception in logging - continue anyway
            try {
                AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                    "LOGGING_ERROR",
                    "Unknown exception while logging accumulated bars",
                    "trading_system.log"
                );
            } catch (...) {
                // Logging failed - continue
            }
        }
    }
    
    return accumulatedBarsResult;
}

double PolygonCryptoClient::get_current_price(const std::string& symbol) const {
    if (!is_connected()) {
        throw std::runtime_error("Polygon client not connected");
    }
    
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for price request");
    }
    
    std::lock_guard<std::mutex> lock(data_mutex);
    auto price_iterator = latest_prices.find(symbol);
    if (price_iterator != latest_prices.end()) {
        return price_iterator->second;
    }
    
    std::string polygon_format_symbol = convert_symbol_to_polygon_format(symbol);
    std::string request_url = build_rest_url(config.endpoints.quotes_latest, symbol);
    std::string response = make_authenticated_request(request_url);
    
    try {
        json response_json = json::parse(response);
        
        if (!response_json.contains("results") || !response_json["results"].is_object()) {
            std::string error_details = "Invalid response format from Polygon.io quotes API. ";
            error_details += "Symbol: " + symbol + " (converted: " + polygon_format_symbol + "). ";
            error_details += "Response keys: ";
            if (response_json.is_object()) {
                bool first = true;
                for (auto& el : response_json.items()) {
                    if (!first) error_details += ", ";
                    error_details += el.key();
                    first = false;
                }
            }
            error_details += ". Response preview: " + response.substr(0, std::min(256UL, response.size()));
            throw std::runtime_error(error_details);
        }
        
        const auto& results = response_json["results"];
        if (!results.contains("last") || !results["last"].contains("price")) {
            throw std::runtime_error("Price data not found in Polygon.io response");
        }
        
        return results["last"]["price"].get<double>();
        
    } catch (const std::exception& exception_error) {
        throw std::runtime_error("Failed to parse Polygon.io price response: " + std::string(exception_error.what()));
    }
}

Core::QuoteData PolygonCryptoClient::get_realtime_quotes(const std::string& symbol) const {
    if (!is_connected()) {
        throw std::runtime_error("Polygon client not connected");
    }
    
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for quote request");
    }
    
    std::lock_guard<std::mutex> lock(data_mutex);
    auto quote_iterator = latest_quotes.find(symbol);
    if (quote_iterator != latest_quotes.end()) {
        return quote_iterator->second;
    }
    
    std::string polygon_format_symbol = convert_symbol_to_polygon_format(symbol);
    std::string request_url = build_rest_url(config.endpoints.quotes_latest, symbol);
    std::string response = make_authenticated_request(request_url);
    
    Core::QuoteData quote_data;
    
    try {
        json response_json = json::parse(response);
        
        if (!response_json.contains("results") || !response_json["results"].is_object()) {
            std::string error_details = "Invalid response format from Polygon.io quotes API. ";
            error_details += "Symbol: " + symbol + " (converted: " + polygon_format_symbol + "). ";
            error_details += "Response keys: ";
            if (response_json.is_object()) {
                bool first = true;
                for (auto& el : response_json.items()) {
                    if (!first) error_details += ", ";
                    error_details += el.key();
                    first = false;
                }
            }
            error_details += ". Response preview: " + response.substr(0, std::min(256UL, response.size()));
            throw std::runtime_error(error_details);
        }
        
        const auto& results = response_json["results"];
        if (results.contains("last")) {
            const auto& last = results["last"];
            if (last.contains("price")) {
                double price = last["price"].get<double>();
                quote_data.ask_price = price;
                quote_data.bid_price = price;
                quote_data.mid_price = price;
            }
            if (last.contains("timestamp")) {
                quote_data.timestamp = std::to_string(last["timestamp"].get<long long>());
            }
        }
        
    } catch (const std::exception& exception_error) {
        throw std::runtime_error("Failed to parse Polygon.io quote response: " + std::string(exception_error.what()));
    }
    
    return quote_data;
}

bool PolygonCryptoClient::is_market_open() const {
    return true;
}

bool PolygonCryptoClient::is_within_trading_hours() const {
    return true;
}

std::string PolygonCryptoClient::get_provider_name() const {
    return "Polygon.io Crypto";
}

Config::ApiProvider PolygonCryptoClient::get_provider_type() const {
    return Config::ApiProvider::POLYGON_CRYPTO;
}

bool PolygonCryptoClient::start_realtime_feed(const std::vector<std::string>& symbols) {
    std::lock_guard<std::mutex> dataGuard(data_mutex);
    
    try {
    if (!is_connected()) {
        return false;
    }
    
    if (symbols.empty()) {
        throw std::runtime_error("At least one symbol is required for realtime feed");
    }
        
        if (config.websocket_url.empty()) {
            throw std::runtime_error("WebSocket URL not configured");
        }
        
        if (config.api_key.empty()) {
            throw std::runtime_error("API key not configured for WebSocket authentication");
    }
    
    if (websocket_active.load()) {
        stop_realtime_feed();
    }
    
        subscribed_symbols = symbols;
        
        if (config.websocket_bar_accumulation_seconds <= 0) {
            throw std::runtime_error("websocket_bar_accumulation_seconds must be configured and greater than 0");
        }
        
        if (config.websocket_second_level_accumulation_seconds <= 0) {
            throw std::runtime_error("websocket_second_level_accumulation_seconds must be configured and greater than 0");
        }
        
        if (config.websocket_max_bar_history_size <= 0) {
            throw std::runtime_error("websocket_max_bar_history_size must be configured and greater than 0");
        }
        
        for (const std::string& symbolString : subscribed_symbols) {
            barAccumulatorMap[symbolString] = std::make_unique<Polygon::BarAccumulator>(
                config.websocket_bar_accumulation_seconds,
                config.websocket_second_level_accumulation_seconds,
                config.websocket_max_bar_history_size
            );
        }
        
        websocketClientPointer = std::make_unique<Polygon::WebSocketClient>();
        
        auto messageCallbackLambda = [this](const std::string& messageContentString) -> bool {
            return this->process_websocket_message(messageContentString);
        };
        
        websocketClientPointer->setMessageCallback(messageCallbackLambda);
        
        if (!websocketClientPointer->connect(config.websocket_url)) {
            std::string connectionErrorString = websocketClientPointer->getLastError();
            websocketClientPointer.reset();
            throw std::runtime_error("WebSocket connection failed: " + connectionErrorString);
        }
        
        bool authenticateResultValue = websocketClientPointer->authenticate(config.api_key);
        
        if (!authenticateResultValue) {
            std::string authenticationErrorString = websocketClientPointer->getLastError();
            websocketClientPointer->disconnect();
            websocketClientPointer.reset();
            throw std::runtime_error("WebSocket authentication failed: " + authenticationErrorString);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::string subscriptionParamsString;
        if (subscribed_symbols.size() == 1) {
            std::string websocketSymbolString = convert_symbol_for_websocket(subscribed_symbols[0]);
            subscriptionParamsString = "XAS." + websocketSymbolString;
        } else {
            subscriptionParamsString = "XAS.*";
        }
        
        if (!websocketClientPointer->subscribe(subscriptionParamsString)) {
            std::string subscriptionErrorString = websocketClientPointer->getLastError();
            websocketClientPointer->disconnect();
            websocketClientPointer.reset();
            throw std::runtime_error("WebSocket subscription failed: " + subscriptionErrorString);
        }
        
        websocketClientPointer->startReceiveLoop();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    websocket_active = true;
    
    return true;
        
    } catch (const std::exception& startFeedExceptionError) {
        barAccumulatorMap.clear();
        if (websocketClientPointer) {
            websocketClientPointer->disconnect();
            websocketClientPointer.reset();
        }
        websocket_active = false;
        throw;
    } catch (...) {
        barAccumulatorMap.clear();
        if (websocketClientPointer) {
            websocketClientPointer->disconnect();
            websocketClientPointer.reset();
        }
        websocket_active = false;
        throw std::runtime_error("Unknown error starting realtime feed");
    }
}

void PolygonCryptoClient::stop_realtime_feed() {
    std::lock_guard<std::mutex> dataGuard(data_mutex);
    
    try {
    websocket_active = false;
    
        if (websocketClientPointer) {
            websocketClientPointer->stopReceiveLoop();
            websocketClientPointer->disconnect();
            websocketClientPointer.reset();
    }
        
        barAccumulatorMap.clear();
    } catch (const std::exception& stopFeedExceptionError) {
        barAccumulatorMap.clear();
        websocketClientPointer.reset();
        websocket_active = false;
    } catch (...) {
        barAccumulatorMap.clear();
        websocketClientPointer.reset();
        websocket_active = false;
    }
}

bool PolygonCryptoClient::process_websocket_message(const std::string& message) {
    try {
        if (message.empty()) {
            return false;
        }
        
        json msg_json;
        try {
            msg_json = json::parse(message);
        } catch (const json::parse_error& parseError) {
            try {
                AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details("PARSE_ERROR", 
                    "Failed to parse JSON: " + std::string(parseError.what()) + ", Message: " + message.substr(0, 200),
                    "trading_system.log");
            } catch (...) {
                // Logging failed, continue
            }
            return false;
        }
        
        if (msg_json.is_array() && !msg_json.empty()) {
            bool allProcessed = true;
            for (size_t i = 0; i < msg_json.size(); ++i) {
                if (msg_json[i].is_object()) {
                    bool elementProcessed = process_single_message(msg_json[i]);
                    if (!elementProcessed) {
                        allProcessed = false;
                    }
                }
            }
            return allProcessed;
        } else if (msg_json.is_object()) {
            return process_single_message(msg_json);
        } else {
            try {
                AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details("UNKNOWN_FORMAT", 
                    "Message is neither array nor object: " + message.substr(0, 200),
                    "trading_system.log");
            } catch (...) {
                // Logging failed, continue
            }
            return false;
        }
        
    } catch (const std::exception& exceptionError) {
        try {
            AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details("EXCEPTION", 
                "Exception in process_websocket_message: " + std::string(exceptionError.what()),
                "trading_system.log");
        } catch (...) {
            // Logging failed, continue
        }
        return false;
    } catch (...) {
        try {
            AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details("UNKNOWN_EXCEPTION", 
                "Unknown exception in process_websocket_message",
                "trading_system.log");
        } catch (...) {
            // Logging failed, continue
        }
        return false;
    }
}

bool PolygonCryptoClient::process_single_message(const json& msg_json) {
    try {
        if (!msg_json.is_object()) {
            return false;
        }
        
        std::string eventTypeString = msg_json.value("ev", "");
        
        if (eventTypeString == "status") {
            return true;
        }
        
        if (eventTypeString == "XAS") {
            std::string pair = msg_json.value("pair", "");
            if (!pair.empty()) {
                // Convert BTC-USD format to BTC/USD for internal use
                std::string internal_symbol = pair;
                size_t dash_pos = internal_symbol.find('-');
                if (dash_pos != std::string::npos) {
                    internal_symbol.replace(dash_pos, 1, "/");
                }
                
                Core::Bar incomingBarData;
                incomingBarData.open_price = msg_json.value("o", 0.0);
                incomingBarData.high_price = msg_json.value("h", 0.0);
                incomingBarData.low_price = msg_json.value("l", 0.0);
                incomingBarData.close_price = msg_json.value("c", 0.0);
                incomingBarData.volume = msg_json.value("v", 0.0);
                
                long long startTimestampValue = msg_json.value("s", 0LL);
                
                if (startTimestampValue <= 0) {
                    try {
                        AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details("INVALID_TIMESTAMP", 
                            "Received invalid timestamp (<= 0): " + std::to_string(startTimestampValue) + " for symbol " + internal_symbol + ". Skipping bar.",
                            "trading_system.log");
                    } catch (...) {
                        // Logging failed, continue
                    }
                    return false;
                }
                
                incomingBarData.timestamp = std::to_string(startTimestampValue);
                
                try {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    
                    auto accumulatorIterator = barAccumulatorMap.find(internal_symbol);
                    if (accumulatorIterator != barAccumulatorMap.end() && accumulatorIterator->second) {
                        try {
                            accumulatorIterator->second->addBar(incomingBarData);
                            
                           
                            
                            try {
                                std::vector<Core::Bar> recentAccumulatedBars = accumulatorIterator->second->getAccumulatedBars(1);
                                if (!recentAccumulatedBars.empty()) {
                                    latest_bars[internal_symbol] = recentAccumulatedBars.back();
                                } else {
                                    latest_bars[internal_symbol] = incomingBarData;
                                }
                            } catch (const std::exception& getBarsExceptionError) {
                                try {
                                    AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                                        "GET_BARS_ERROR",
                                        "Failed to get accumulated bars: " + std::string(getBarsExceptionError.what()),
                                        "trading_system.log"
                                    );
                                } catch (...) {
                                    // Logging failed, continue
                                }
                                latest_bars[internal_symbol] = incomingBarData;
                            } catch (...) {
                                latest_bars[internal_symbol] = incomingBarData;
                            }
                        } catch (const std::exception& accumulatorExceptionError) {
                            try {
                                AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                                    "ACCUMULATOR_ERROR",
                                    "Failed to process accumulator: " + std::string(accumulatorExceptionError.what()),
                                    "trading_system.log"
                                );
                            } catch (...) {
                                // Logging failed, continue
                            }
                            latest_bars[internal_symbol] = incomingBarData;
                        } catch (...) {
                            latest_bars[internal_symbol] = incomingBarData;
                        }
                    } else {
                        latest_bars[internal_symbol] = incomingBarData;
                        try {
                            AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                                "NO_ACCUMULATOR",
                                "No accumulator found for symbol: " + internal_symbol + ", using incoming bar directly",
                                "trading_system.log"
                            );
                        } catch (...) {
                            // Logging failed, continue
                        }
                    }
                    
                    try {
                        latest_prices[internal_symbol] = incomingBarData.close_price;
                    } catch (const std::exception& priceUpdateExceptionError) {
                        try {
                            AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                                "PRICE_UPDATE_ERROR",
                                "Failed to update price: " + std::string(priceUpdateExceptionError.what()),
                                "trading_system.log"
                            );
                        } catch (...) {
                            // Logging failed, continue
                        }
                    } catch (...) {
                        // Price update failed, continue
                    }
                    
                    return true;
                } catch (const std::exception& lockExceptionError) {
                    try {
                        AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                            "LOCK_ERROR",
                            "Failed to acquire lock or process bar data: " + std::string(lockExceptionError.what()),
                            "trading_system.log"
                        );
                    } catch (...) {
                        // Logging failed, continue
                    }
                    return false;
                } catch (...) {
                    try {
                        AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                            "UNKNOWN_LOCK_ERROR",
                            "Unknown error processing bar data",
                            "trading_system.log"
                        );
                    } catch (...) {
                        // Logging failed, continue
                    }
                    return false;
                }
            }
        }
        
        // Handle quote messages (Q) - existing functionality
        if (msg_json.contains("ev") && msg_json["ev"] == "Q") {
            std::string symbol = msg_json.value("sym", "");
            if (!symbol.empty()) {
                try {
                Core::QuoteData quote;
                quote.ask_price = msg_json.value("ap", 0.0);
                quote.bid_price = msg_json.value("bp", 0.0);
                quote.ask_size = msg_json.value("as", 0.0);
                quote.bid_size = msg_json.value("bs", 0.0);
                quote.timestamp = std::to_string(msg_json.value("t", 0LL));
                quote.mid_price = (quote.ask_price + quote.bid_price) / 2.0;
                
                    try {
                std::lock_guard<std::mutex> lock(data_mutex);
                latest_quotes[symbol] = quote;
                latest_prices[symbol] = quote.mid_price;
                return true;
                    } catch (const std::exception& lockExceptionError) {
                        try {
                            AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                                "QUOTE_LOCK_ERROR",
                                "Failed to acquire lock for quote: " + std::string(lockExceptionError.what()),
                                "trading_system.log"
                            );
                        } catch (...) {
                            // Logging failed, continue
                        }
                        return false;
                    } catch (...) {
                        try {
                            AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                                "QUOTE_UNKNOWN_LOCK_ERROR",
                                "Unknown error acquiring lock for quote",
                                "trading_system.log"
                            );
                        } catch (...) {
                            // Logging failed, continue
                        }
                        return false;
                    }
                } catch (const std::exception& quoteExceptionError) {
                    try {
                        AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                            "QUOTE_PARSE_ERROR",
                            "Failed to parse quote data: " + std::string(quoteExceptionError.what()),
                            "trading_system.log"
                        );
                    } catch (...) {
                        // Logging failed, continue
                    }
                    return false;
                } catch (...) {
                    try {
                        AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                            "QUOTE_UNKNOWN_ERROR",
                            "Unknown error parsing quote data",
                            "trading_system.log"
                        );
                    } catch (...) {
                        // Logging failed, continue
                    }
                    return false;
                }
            }
        }
        
        return false;
    } catch (const std::exception& processMessageExceptionError) {
        try {
            AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                "PROCESS_MESSAGE_ERROR",
                "Exception in process_single_message: " + std::string(processMessageExceptionError.what()),
                "trading_system.log"
            );
        } catch (...) {
            // Logging failed, continue
        }
        return false;
    } catch (...) {
        try {
            AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                "PROCESS_MESSAGE_UNKNOWN_ERROR",
                "Unknown exception in process_single_message",
                "trading_system.log"
            );
        } catch (...) {
            // Logging failed, continue
        }
        return false;
    }
}

std::string PolygonCryptoClient::convert_symbol_for_websocket(const std::string& symbol) const {
    // Convert BTC/USD to BTC-USD for WebSocket subscription
    std::string ws_symbol = symbol;
    size_t slash_pos = ws_symbol.find('/');
    if (slash_pos != std::string::npos) {
        ws_symbol.replace(slash_pos, 1, "-");
    }
    return ws_symbol;
}

std::string PolygonCryptoClient::convert_symbol_to_polygon_format(const std::string& symbol) const {
    if (symbol.empty()) {
        return symbol;
    }
    
    std::string polygon_symbol = symbol;
    
    // Convert BTC/USD to X:BTCUSD format for crypto
    // Polygon/Massive crypto format: X:BTCUSD (X: prefix + no separator)
    size_t slash_position = polygon_symbol.find('/');
    if (slash_position != std::string::npos) {
        std::string base_currency = polygon_symbol.substr(0, slash_position);
        std::string quote_currency = polygon_symbol.substr(slash_position + 1);
        
        // Remove any existing X: prefix if present
        if (base_currency.find("X:") == 0) {
            base_currency = base_currency.substr(2);
        }
        
        // Format as X:BTCUSD (X: prefix + base + quote, no separator)
        polygon_symbol = "X:" + base_currency + quote_currency;
    } else {
        // If no slash, check if it already has X: prefix
        if (polygon_symbol.find("X:") != 0 && polygon_symbol.find("x:") != 0) {
            // Add X: prefix if missing
            polygon_symbol = "X:" + polygon_symbol;
        }
    }
    
    return polygon_symbol;
}

std::string PolygonCryptoClient::build_rest_url(const std::string& endpoint, const std::string& symbol) const {
    std::string request_url = config.base_url + endpoint;
    
    std::string polygon_format_symbol = convert_symbol_to_polygon_format(symbol);
    request_url = replace_url_placeholder(request_url, polygon_format_symbol);
    
    if (endpoint.find("range") != std::string::npos) {
        size_t multiplier_position = request_url.find("{multiplier}");
        if (multiplier_position != std::string::npos) {
            if (config.bar_multiplier <= 0) {
                throw std::runtime_error("Polygon bar_multiplier must be configured and > 0");
            }
            std::string multiplier_string = std::to_string(config.bar_multiplier);
            request_url.replace(multiplier_position, 12, multiplier_string);
        }
        
        size_t timespan_position = request_url.find("{timespan}");
        if (timespan_position != std::string::npos) {
            if (config.bar_timespan.empty()) {
                throw std::runtime_error("Polygon bar_timespan must be configured (e.g., minute, hour, day)");
            }
            request_url.replace(timespan_position, 10, config.bar_timespan);
        }
        
        if (config.bars_range_minutes <= 0) {
            throw std::runtime_error("Polygon bars_range_minutes must be configured and > 0");
        }
        auto current_time = std::chrono::system_clock::now();
        auto from_time_point = current_time - std::chrono::minutes(config.bars_range_minutes);
        auto current_time_t = std::chrono::system_clock::to_time_t(current_time);
        auto from_time_t = std::chrono::system_clock::to_time_t(from_time_point);
        
        // Use date format (YYYY-MM-DD) for free tier compatibility
        // Convert timestamps to date strings
        std::stringstream from_ss, to_ss;
        std::tm from_tm, to_tm;
        
        // Use thread-safe gmtime_r
        gmtime_r(&from_time_t, &from_tm);
        gmtime_r(&current_time_t, &to_tm);
        
        from_ss << std::put_time(&from_tm, "%Y-%m-%d");
        to_ss << std::put_time(&to_tm, "%Y-%m-%d");
        
        std::string from_date_string = from_ss.str();
        std::string to_date_string = to_ss.str();
        
        size_t from_position = request_url.find("{from}");
        if (from_position != std::string::npos) {
            request_url.replace(from_position, 6, from_date_string);
        }
        
        size_t to_position = request_url.find("{to}");
        if (to_position != std::string::npos) {
            request_url.replace(to_position, 4, to_date_string);
        }
    }
    
    request_url += (request_url.find('?') != std::string::npos) ? "&" : "?";
    request_url += "apiKey=" + config.api_key;
    
    return request_url;
}

std::string PolygonCryptoClient::make_authenticated_request(const std::string& request_url) const {
    if (request_url.empty()) {
        throw std::runtime_error("URL is required for authenticated request");
    }
    
    try {
        HttpRequest http_request(request_url, "", "", config.retry_count, config.timeout_seconds, 
                           config.enable_ssl_verification, config.rate_limit_delay_ms, "");
        
        std::string response = http_get(http_request, connectivity_manager);
        
        if (response.empty()) {
            throw std::runtime_error("Empty response from Polygon.io API");
        }
        
        if (response.find("404") != std::string::npos || response.find("page not found") != std::string::npos) {
            throw std::runtime_error("Polygon API endpoint not found (404). Verify symbol format (use BTC-USD not BTC/USD) and endpoint configuration. Response: " + (response.size() > 128 ? response.substr(0, 128) : response));
        }
        
        if (!response.empty() && (response.rfind("Bad Request", 0) == 0 || response.rfind("400", 0) == 0 || response[0] != '{')) {
            throw std::runtime_error("Polygon API returned error payload: " + (response.size() > 64 ? response.substr(0, 64) : response));
        }
        
        return response;
    } catch (const std::exception& exception_error) {
        throw std::runtime_error("Polygon API request failed: " + std::string(exception_error.what()) + " for URL: " + request_url);
    }
}

bool PolygonCryptoClient::validate_config() const {
    return true;
}

void PolygonCryptoClient::cleanup_resources() {
    stop_realtime_feed();
    
    std::lock_guard<std::mutex> lock(data_mutex);
    latest_quotes.clear();
    latest_prices.clear();
}

} // namespace API
} // namespace AlpacaTrader
