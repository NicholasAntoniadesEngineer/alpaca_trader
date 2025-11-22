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
        
        try {
        bool startResultValue = const_cast<PolygonCryptoClient*>(this)->start_realtime_feed(symbolVector);
        
        if (!startResultValue) {
                throw std::runtime_error("CRITICAL: Failed to start WebSocket real-time feed for symbol: " + request.symbol);
            }
        } catch (const std::exception& start_feed_exception_error) {
            throw std::runtime_error("CRITICAL: WebSocket real-time feed startup exception: " + std::string(start_feed_exception_error.what()));
        } catch (...) {
            throw std::runtime_error("CRITICAL: Unknown error starting WebSocket real-time feed for symbol: " + request.symbol);
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

        // CRITICAL FIX: Initialize accumulator with historical bars if it's empty
        // This ensures ATR has sufficient data even if WebSocket was already started by MTH-TS
        if (accumulatorPointer->getAccumulatedBarsCount() == 0) {
            try {
                // COMPLIANCE: Load much more recent historical data for fresh trading decisions
                // Load last 30 minutes of 1-second bars for maximum recency
                auto now = std::chrono::system_clock::now();
                auto thirty_minutes_ago = now - std::chrono::minutes(30);

                // Polygon.io expects Unix timestamps in milliseconds
                auto start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(thirty_minutes_ago.time_since_epoch()).count();
                auto end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

                std::string start_timestamp = std::to_string(start_ms);
                std::string end_timestamp = std::to_string(end_ms);

                // Load 1-second bars (1800 bars for 30 minutes) for fresh data
                std::vector<Core::Bar> historicalBars = const_cast<PolygonCryptoClient*>(this)->get_historical_bars(
                    request.symbol, "1sec", start_timestamp, end_timestamp, 1800);

                if (!historicalBars.empty()) {
                    // Add 1-second bars directly to accumulator for maximum data freshness
                    for (const auto& bar : historicalBars) {
                        accumulatorPointer->addBar(bar);
                    }
                    AlpacaTrader::Logging::log_message("WebSocket accumulator initialized with " + std::to_string(historicalBars.size()) + " fresh historical 1sec bars (<30min old) for " + request.symbol, "");
                } else {
                    AlpacaTrader::Logging::log_message("CRITICAL: No historical bars available to initialize WebSocket accumulator for " + request.symbol, "");
                }
            } catch (const std::exception& e) {
                AlpacaTrader::Logging::log_message("WARNING: Failed to load historical bars for accumulator initialization: " + std::string(e.what()), "");
            }
    }
    
    size_t accumulatorBarCountValue = accumulatorPointer->getAccumulatedBarsCount();
    
    std::vector<Core::Bar> accumulatedBarsResult = accumulatorPointer->getAccumulatedBars(request.limit);
    if (accumulatedBarsResult.empty()) {
        // SPECIAL HANDLING FOR MTH-TS: Check if MTH-TS is enabled and has historical data
        bool mth_ts_has_data = false;
        if (multi_timeframe_manager) {
            try {
                const auto& mtf_data = multi_timeframe_manager->get_multi_timeframe_data();
                // Check if we have sufficient historical data for MTH-TS
                mth_ts_has_data = (static_cast<int>(mtf_data.second_bars.size()) >= 10); // At least 10 1-second bars
            } catch (const std::exception& e) {
                std::string error_msg = "CRITICAL: MTH-TS historical data check failed - system cannot continue safely. Error: " + std::string(e.what());
                std::cerr << error_msg << std::endl;
                throw std::runtime_error(error_msg);
            } catch (...) {
                std::string error_msg = "CRITICAL: MTH-TS historical data check failed with unknown error - system cannot continue safely";
                std::cerr << error_msg << std::endl;
                throw std::runtime_error(error_msg);
            }
        }

        if (mth_ts_has_data) {
            // MTH-TS has historical data, allow system to proceed without accumulated bars
            // Only log this once per minute to avoid spam
            static auto last_log_time = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            auto time_since_last_log = std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time).count();
            if (time_since_last_log >= 60) {
                std::cerr << "MTH-TS: No accumulated bars but historical data available - proceeding with MTH-TS" << std::endl;
                last_log_time = now;
            }
            return {}; // Return empty vector, MTH-TS will use historical data
        } else {
            // COMPLIANCE LAW: No fallbacks ever - fail hard with proper logging
            std::string errorMessageString = "CRITICAL: WebSocket is active but no accumulated bars available. ";
        errorMessageString += "Accumulator has: " + std::to_string(accumulatorBarCountValue) + " bars. ";
            errorMessageString += "System requires real market data - no synthetic/fallback bars allowed. ";
            errorMessageString += "Waiting for real-time bar data to accumulate from Polygon.io. ";
            errorMessageString += "Required accumulation time: " + std::to_string(config.websocket_bar_accumulation_seconds) + " seconds.";

            // LOG CRITICAL ERROR AND FAIL HARD - NO FALLBACKS ALLOWED
            std::cerr << "CRITICAL: " << errorMessageString << std::endl;
        throw std::runtime_error(errorMessageString);
    }
    }
    
    // FIRST PRINCIPLES: Crypto markets are 24/7 - staleness logic must be different than stock markets
    // Don't check bar age (crypto never stops trading), check WebSocket connectivity instead

    // Check if WebSocket is actively providing data (primary indicator of data freshness for crypto)
    bool websocketCurrentlyActive = websocket_active.load();
    bool clientCurrentlyConnected = is_connected();

    if (!websocketCurrentlyActive || !clientCurrentlyConnected) {
        std::string websocketInactiveErrorString = "CRITICAL: WebSocket connection lost for crypto market. ";
        websocketInactiveErrorString += "WebSocket Active: " + std::string(websocketCurrentlyActive ? "YES" : "NO") + ". ";
        websocketInactiveErrorString += "Client Connected: " + std::string(clientCurrentlyConnected ? "YES" : "NO") + ". ";
        websocketInactiveErrorString += "Crypto markets require continuous real-time data. Total accumulated bars: " + std::to_string(accumulatedBarsResult.size()) + ".";
        throw std::runtime_error(websocketInactiveErrorString);
    }
    
    // SECONDARY CHECK: Validate latest bar timestamp (but don't treat age as staleness for 24/7 markets)
    auto currentTimeMillisValue = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
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
        
        // RELAXED CHECK: For crypto, only log warnings if bars are extremely old (>1 hour)
        // Don't treat as "stale" since crypto trades 24/7
        long long cryptoBarAgeWarningThresholdMillisValue = 60 * 60 * 1000LL; // 1 hour

        if (latestBarAgeMillisValue > cryptoBarAgeWarningThresholdMillisValue) {
            auto nowTimeValue = std::chrono::steady_clock::now();
            auto timeSinceLastStaleLogValue = std::chrono::duration_cast<std::chrono::seconds>(nowTimeValue - lastStaleDataLogTime).count();
            bool shouldLogStaleDataFlag = (timeSinceLastStaleLogValue >= 60) || (lastStaleDataTimestamp != latestBarValue.timestamp);
            
            if (shouldLogStaleDataFlag) {
                try {
                    // For crypto markets, this is just a warning - not a failure
                    AlpacaTrader::Logging::WebSocketLogs::log_websocket_message_details(
                        "CRYPTO_BAR_AGE_WARNING",
                        "Crypto bar is " + std::to_string(latestBarAgeSecondsValue) + " seconds old (WebSocket active, continuing)",
                        "trading_system.log"
                    );
                    lastStaleDataLogTime = nowTimeValue;
                    lastStaleDataTimestamp = latestBarValue.timestamp;
                } catch (...) {
                    // Logging failed, continue
                }
            }
            
            // IMPORTANT: For crypto markets, don't throw an exception for old bars
            // Crypto trades 24/7, so bars can be "old" but still valid if WebSocket is active
            // The WebSocket connectivity check above is the primary freshness indicator
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
    
    // SPECIAL HANDLING FOR MTH-TS: Allow proceeding with fewer bars if MTH-TS has historical data
    bool allow_mth_ts_proceed = false;
    if (static_cast<int>(accumulatedBarsResult.size()) < request.limit) {
        try {
            // Check if MTH-TS is enabled and has sufficient historical data
            if (multi_timeframe_manager) {
                const auto& mtf_data = multi_timeframe_manager->get_multi_timeframe_data();
                // Allow proceeding if we have at least minimum required historical bars
                allow_mth_ts_proceed = (static_cast<int>(mtf_data.daily_bars.size()) >= 10) &&
                                     (static_cast<int>(mtf_data.thirty_min_bars.size()) >= 10) &&
                                     (static_cast<int>(mtf_data.minute_bars.size()) >= 10) &&
                                     (static_cast<int>(mtf_data.second_bars.size()) >= 10);
            }
        } catch (...) {
            // Ignore MTH-TS check errors
        }

        if (!allow_mth_ts_proceed) {
        std::string insufficientBarsErrorString = "Insufficient accumulated bars available. ";
        insufficientBarsErrorString += "Have " + std::to_string(accumulatedBarsResult.size()) + " bars, need " + std::to_string(request.limit) + ". ";
        insufficientBarsErrorString += "Waiting for more real-time data to accumulate.";
        throw std::runtime_error(insufficientBarsErrorString);
        }
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

std::vector<Core::Bar> PolygonCryptoClient::get_historical_bars(const std::string& symbol_input_parameter,
                                                                const std::string& timeframe_input_parameter,
                                                                const std::string& start_date_input_parameter,
                                                                const std::string& end_date_input_parameter,
                                                                int limit_input_parameter) const {
    // Validate connection status - system must be connected to make API calls
    if (!is_connected()) {
        throw std::runtime_error("CRITICAL: Polygon client not connected - cannot fetch historical bars");
    }

    // Validate required input parameters - all parameters must be provided and valid
    if (symbol_input_parameter.empty()) {
        throw std::runtime_error("CRITICAL: Symbol parameter is required for historical bars request but was empty");
    }

    if (timeframe_input_parameter.empty()) {
        throw std::runtime_error("CRITICAL: Timeframe parameter is required for historical bars request but was empty");
    }

    if (start_date_input_parameter.empty()) {
        throw std::runtime_error("CRITICAL: Start date parameter is required for historical bars request but was empty");
    }

    if (end_date_input_parameter.empty()) {
        throw std::runtime_error("CRITICAL: End date parameter is required for historical bars request but was empty");
    }

    if (limit_input_parameter <= 0) {
        throw std::runtime_error("CRITICAL: Limit parameter must be greater than 0 for historical bars request");
    }

    // Convert symbol to Polygon crypto format (X:BTCUSD)
    std::string polygon_symbol_format = convert_symbol_to_polygon_format(symbol_input_parameter);

    // Parse timeframe to determine multiplier and timespan from config
    int bar_multiplier_value = 1;
    std::string bar_timespan_value = "minute";

    if (timeframe_input_parameter == "1sec") {
        bar_multiplier_value = 1;
        bar_timespan_value = "second";
    } else if (timeframe_input_parameter == "1min") {
        bar_multiplier_value = 1;
        bar_timespan_value = "minute";
    } else if (timeframe_input_parameter == "30min") {
        bar_multiplier_value = 30;
        bar_timespan_value = "minute";
    } else if (timeframe_input_parameter == "1day") {
        bar_multiplier_value = 1;
        bar_timespan_value = "day";
    } else {
        throw std::runtime_error("CRITICAL: Unsupported timeframe '" + timeframe_input_parameter +
                                "' - supported timeframes: 1sec, 1min, 30min, 1day");
    }

    // Build API URL using configured endpoint template
    std::string historical_bars_endpoint = config.endpoints.historical_bars;
    if (historical_bars_endpoint.empty()) {
        throw std::runtime_error("CRITICAL: Historical bars endpoint not configured in api_endpoints_config.csv");
    }

    // Replace placeholders in endpoint template using a helper function
    std::string api_url_construction = config.base_url + historical_bars_endpoint;

    // Helper function to replace placeholders safely
    auto replace_placeholder = [](std::string& str, const std::string& placeholder, const std::string& replacement) {
        size_t pos = str.find(placeholder);
        if (pos != std::string::npos) {
            str.replace(pos, placeholder.length(), replacement);
        }
    };

    // Replace all placeholders
    replace_placeholder(api_url_construction, "{symbol}", polygon_symbol_format);
    replace_placeholder(api_url_construction, "{multiplier}", std::to_string(bar_multiplier_value));
    replace_placeholder(api_url_construction, "{timespan}", bar_timespan_value);
    replace_placeholder(api_url_construction, "{from}", start_date_input_parameter);
    replace_placeholder(api_url_construction, "{to}", end_date_input_parameter);

    // Add query parameters from configuration
    std::string query_parameters = "?adjusted=" + std::string(config.historical_bars_adjusted ? "true" : "false") +
                                  "&sort=" + config.historical_bars_sort +
                                  "&limit=" + std::to_string(std::min(limit_input_parameter, config.historical_bars_limit)) +
                                  "&apiKey=" + config.api_key;

    std::string final_api_url = api_url_construction + query_parameters;

    // Execute HTTP request to Polygon API
    HttpRequest historical_bars_request(final_api_url, config.api_key, "", config.retry_count,
                                       config.timeout_seconds, config.enable_ssl_verification,
                                       config.rate_limit_delay_ms, "");

    std::string polygon_api_response;
    try {
        polygon_api_response = http_get(historical_bars_request, connectivity_manager);
    } catch (const std::exception& http_request_exception) {
        throw std::runtime_error("CRITICAL: HTTP request failed for historical bars - " +
                                std::string(http_request_exception.what()) +
                                " | URL: " + final_api_url.substr(0, 100) + "...");
    }

    if (polygon_api_response.empty()) {
        throw std::runtime_error("CRITICAL: Empty response received from Polygon API for historical bars | Symbol: " +
                                symbol_input_parameter + " | Timeframe: " + timeframe_input_parameter);
    }

    // Parse JSON response from Polygon API
    nlohmann::json parsed_json_response;
    try {
        parsed_json_response = nlohmann::json::parse(polygon_api_response);
    } catch (const std::exception& json_parse_exception) {
        throw std::runtime_error("CRITICAL: Failed to parse JSON response from Polygon API - " +
                                std::string(json_parse_exception.what()) +
                                " | Response preview: " + polygon_api_response.substr(0, 200));
    }

    // Validate API response structure
    if (parsed_json_response.contains("error")) {
        std::string api_error_message = parsed_json_response["error"];
        throw std::runtime_error("CRITICAL: Polygon API returned error - " + api_error_message +
                                " | Symbol: " + symbol_input_parameter + " | Timeframe: " + timeframe_input_parameter);
    }

    if (!parsed_json_response.contains("results")) {
        throw std::runtime_error("CRITICAL: Polygon API response missing 'results' field | Response: " +
                                polygon_api_response.substr(0, 200));
    }

    if (!parsed_json_response["results"].is_array()) {
        throw std::runtime_error("CRITICAL: Polygon API 'results' field is not an array | Response: " +
                                polygon_api_response.substr(0, 200));
    }

    // Process each bar in the results array
    std::vector<Core::Bar> historical_bars_collection;
    const auto& results_array = parsed_json_response["results"];

    for (const auto& individual_bar_data : results_array) {
        // Validate bar data structure - all required fields must be present and valid
        if (!individual_bar_data.contains("o") || !individual_bar_data.contains("h") ||
            !individual_bar_data.contains("l") || !individual_bar_data.contains("c") ||
            !individual_bar_data.contains("v") || !individual_bar_data.contains("t")) {
            throw std::runtime_error("CRITICAL: Incomplete bar data in Polygon API response - missing required OHLCV or timestamp fields");
        }

        // Extract and validate price data
        double open_price_value = individual_bar_data["o"];
        double high_price_value = individual_bar_data["h"];
        double low_price_value = individual_bar_data["l"];
        double close_price_value = individual_bar_data["c"];
        double volume_value = individual_bar_data["v"];

        if (open_price_value <= 0.0 || high_price_value <= 0.0 || low_price_value <= 0.0 || close_price_value <= 0.0) {
            throw std::runtime_error("CRITICAL: Invalid bar data - OHLC prices must be positive | O:" +
                                    std::to_string(open_price_value) + " H:" + std::to_string(high_price_value) +
                                    " L:" + std::to_string(low_price_value) + " C:" + std::to_string(close_price_value));
        }

        // Validate logical price relationships
        if (high_price_value < low_price_value || high_price_value < open_price_value || high_price_value < close_price_value ||
            low_price_value > open_price_value || low_price_value > close_price_value) {
            throw std::runtime_error("CRITICAL: Invalid bar data - price relationships violated | H:" +
                                    std::to_string(high_price_value) + " must be >= L:" + std::to_string(low_price_value));
        }

        // Extract timestamp (Polygon returns milliseconds since Unix epoch)
        long long timestamp_milliseconds = individual_bar_data["t"];

        // Create bar structure with validated data
        Core::Bar validated_bar_structure;
        validated_bar_structure.open_price = open_price_value;
        validated_bar_structure.high_price = high_price_value;
        validated_bar_structure.low_price = low_price_value;
        validated_bar_structure.close_price = close_price_value;
        validated_bar_structure.volume = volume_value;
        validated_bar_structure.timestamp = std::to_string(timestamp_milliseconds);

        historical_bars_collection.push_back(validated_bar_structure);
    }

    // Validate that we received at least some bars
    if (historical_bars_collection.empty()) {
        throw std::runtime_error("CRITICAL: No valid bars received from Polygon API | Symbol: " +
                                symbol_input_parameter + " | Timeframe: " + timeframe_input_parameter +
                                " | Start: " + start_date_input_parameter + " | End: " + end_date_input_parameter);
    }

    return historical_bars_collection;
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
        
        try {
        websocketClientPointer = std::make_unique<Polygon::WebSocketClient>();
        } catch (const std::exception& websocket_create_exception_error) {
            throw std::runtime_error("CRITICAL: Failed to create WebSocket client: " + std::string(websocket_create_exception_error.what()));
        } catch (...) {
            throw std::runtime_error("CRITICAL: Unknown error creating WebSocket client");
        }
        
        try {
        auto messageCallbackLambda = [this](const std::string& messageContentString) -> bool {
            return this->process_websocket_message(messageContentString);
        };
        
        websocketClientPointer->setMessageCallback(messageCallbackLambda);
        } catch (const std::exception& callback_exception_error) {
            websocketClientPointer.reset();
            throw std::runtime_error("CRITICAL: Failed to set WebSocket message callback: " + std::string(callback_exception_error.what()));
        } catch (...) {
            websocketClientPointer.reset();
            throw std::runtime_error("CRITICAL: Unknown error setting WebSocket message callback");
        }

        try {
        if (!websocketClientPointer->connect(config.websocket_url)) {
            std::string connectionErrorString = websocketClientPointer->getLastError();
            websocketClientPointer.reset();
                throw std::runtime_error("CRITICAL: WebSocket connection failed: " + connectionErrorString);
        }
        } catch (const std::exception& connect_exception_error) {
            websocketClientPointer.reset();
            throw std::runtime_error("CRITICAL: WebSocket connection exception: " + std::string(connect_exception_error.what()));
        } catch (...) {
            websocketClientPointer.reset();
            throw std::runtime_error("CRITICAL: Unknown WebSocket connection error");
        }
        
        try {
        bool authenticateResultValue = websocketClientPointer->authenticate(config.api_key);
        
        if (!authenticateResultValue) {
            std::string authenticationErrorString = websocketClientPointer->getLastError();
            websocketClientPointer->disconnect();
            websocketClientPointer.reset();
                throw std::runtime_error("CRITICAL: WebSocket authentication failed: " + authenticationErrorString);
            }
        } catch (const std::exception& auth_exception_error) {
            websocketClientPointer->disconnect();
            websocketClientPointer.reset();
            throw std::runtime_error("CRITICAL: WebSocket authentication exception: " + std::string(auth_exception_error.what()));
        } catch (...) {
            websocketClientPointer->disconnect();
            websocketClientPointer.reset();
            throw std::runtime_error("CRITICAL: Unknown WebSocket authentication error");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::string subscriptionParamsString;
        if (subscribed_symbols.size() == 1) {
            std::string websocketSymbolString = convert_symbol_for_websocket(subscribed_symbols[0]);
            // Subscribe to 1-second bars (XAS) and quotes (XQ) for crypto
            // XAS = Aggregates Per Second (1-second OHLCV bars)
            // XQ = Real-time quotes (bid/ask updates)
            subscriptionParamsString = "XAS." + websocketSymbolString + ",XQ." + websocketSymbolString;
        } else {
            // Multi-symbol: Subscribe to 1-second aggregates (XAS) and quotes (XQ) for all symbols
            subscriptionParamsString = "XAS.*,XQ.*";
        }
        
        if (!websocketClientPointer->subscribe(subscriptionParamsString)) {
            std::string subscriptionErrorString = websocketClientPointer->getLastError();
            websocketClientPointer->disconnect();
            websocketClientPointer.reset();
            throw std::runtime_error("WebSocket subscription failed: " + subscriptionErrorString);
        }
        
        try {
            // Start WebSocket receive loop
        websocketClientPointer->startReceiveLoop();
        } catch (const std::exception& receive_loop_exception_error) {
            websocketClientPointer->disconnect();
            websocketClientPointer.reset();
            throw std::runtime_error("CRITICAL: Failed to start WebSocket receive loop: " + std::string(receive_loop_exception_error.what()));
        } catch (...) {
            websocketClientPointer->disconnect();
            websocketClientPointer.reset();
            throw std::runtime_error("CRITICAL: Unknown error starting WebSocket receive loop");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    websocket_active = true;

        // Start polling thread as fallback for reliable data collection
        try {
            startPollingThread();
        } catch (const std::exception& polling_exception_error) {
            websocketClientPointer->disconnect();
            websocketClientPointer.reset();
            websocket_active = false;
            throw std::runtime_error("CRITICAL: Failed to start polling thread: " + std::string(polling_exception_error.what()));
        } catch (...) {
            websocketClientPointer->disconnect();
            websocketClientPointer.reset();
            websocket_active = false;
            throw std::runtime_error("CRITICAL: Unknown error starting polling thread");
        }
    
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

        // Stop polling thread first
        stopPollingThread();
    
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
    // ABSOLUTE DEFENSIVE PROGRAMMING - NO EXCEPTIONS CAN ESCAPE THIS FUNCTION
    // This function is called from WebSocket message processing and MUST NOT THROW
    try {
        // Increment message counter for debugging
        messages_processed++;

        // Basic validation
        if (!msg_json.is_object()) {
            std::cerr << "WebSocket: Received non-object message #" << messages_processed << std::endl;
            return false;
        }
        
        // Get event type safely
        std::string event_type;
        try {
            event_type = msg_json.value("ev", "");
        } catch (...) {
            std::cerr << "WebSocket: Failed to get event type in message #" << messages_processed << std::endl;
            return false;
        }


        // Handle different message types
        if (event_type == "status") {
            return true;
        }
        
        // XAS = Aggregates Per Second (1-second bars) - PRIMARY DATA SOURCE
        if (event_type == "XAS") {
            return process_bar_message(msg_json);
        }
        
        // XA = 1-minute aggregates (legacy, may not be used)
        if (event_type == "XA") {
            return process_bar_message(msg_json);
        }

        // XQ = Real-time quotes (bid/ask)
        if (event_type == "XQ") {
            return process_quote_message(msg_json);
        }

        std::cerr << "WebSocket: Unknown message type: " << event_type << std::endl;
        return false;
    } catch (...) {
        // ABSOLUTE CATCH-ALL - NO EXCEPTIONS CAN ESCAPE THIS FUNCTION
        std::cerr << "CRITICAL: Unknown exception in process_single_message - system must continue" << std::endl;
        return false;
    }
}

bool PolygonCryptoClient::process_bar_message(const json& msg_json) {
    // ABSOLUTE DEFENSIVE PROGRAMMING - NO EXCEPTIONS CAN ESCAPE
    try {
        // Get symbol - ABSOLUTELY SAFE
        std::string pair;
        try {
            pair = msg_json.value("pair", "");
                    } catch (...) {
            std::cerr << "BAR: Failed to get pair" << std::endl;
            return false;
                    }

        if (pair.empty()) {
                    return false;
                }
                
        // Convert symbol format - ABSOLUTELY SAFE
        std::string symbol = pair;
        try {
            size_t dash_pos = symbol.find('-');
            if (dash_pos != std::string::npos) {
                symbol.replace(dash_pos, 1, "/");
            }
        } catch (...) {
            std::cerr << "BAR: Symbol conversion failed" << std::endl;
            return false;
        }

        // Extract bar data - EACH ACCESS MUST BE SAFE
        Core::Bar bar;
        try {
            bar.open_price = msg_json.value("o", 0.0);
        } catch (...) {
            std::cerr << "BAR: Failed to get open_price" << std::endl;
            return false;
        }

        try {
            bar.high_price = msg_json.value("h", 0.0);
        } catch (...) {
            std::cerr << "BAR: Failed to get high_price" << std::endl;
            return false;
        }

        try {
            bar.low_price = msg_json.value("l", 0.0);
                                } catch (...) {
            std::cerr << "BAR: Failed to get low_price" << std::endl;
            return false;
                                }

        try {
            bar.close_price = msg_json.value("c", 0.0);
                            } catch (...) {
            std::cerr << "BAR: Failed to get close_price" << std::endl;
            return false;
        }

        try {
            bar.volume = msg_json.value("v", 0.0);
                            } catch (...) {
            std::cerr << "BAR: Failed to get volume" << std::endl;
            return false;
                            }

        long long timestamp_val = 0;
        try {
            timestamp_val = msg_json.value("s", 0LL);
                        } catch (...) {
            std::cerr << "BAR: Failed to get timestamp" << std::endl;
            return false;
        }

        try {
            bar.timestamp = std::to_string(timestamp_val);
                        } catch (...) {
            std::cerr << "BAR: Timestamp to string failed" << std::endl;
            return false;
        }

        // Validate data - ABSOLUTELY SAFE
        if (bar.close_price <= 0.0 || bar.timestamp == "0") {
            std::cerr << "BAR: Invalid data - close_price: " << bar.close_price << ", timestamp: " << bar.timestamp << std::endl;
            return false;
        }


        // Update data structures - ABSOLUTELY SAFE
        try {
            std::lock_guard<std::mutex> lock(data_mutex);
            try {
                latest_bars[symbol] = bar;
                        } catch (...) {
                std::cerr << "BAR: Failed to update latest_bars" << std::endl;
                        }

            try {
                latest_prices[symbol] = bar.close_price;
                    } catch (...) {
                std::cerr << "BAR: Failed to update latest_prices" << std::endl;
            }

            // Add to accumulator if available - SAFE
            try {
                auto it = barAccumulatorMap.find(symbol);
                if (it != barAccumulatorMap.end() && it->second) {
                    it->second->addBar(bar);
                }
                } catch (...) {
                std::cerr << "BAR: Failed to add to accumulator" << std::endl;
            }

            // Process MTH-TS data - THIS IS THE MOST DANGEROUS PART
            if (multi_timeframe_manager) {
                try {
                    Core::MultiTimeframeBar mtf_bar(
                        bar.open_price, bar.high_price, bar.low_price,
                        bar.close_price, bar.volume, 0.0, bar.timestamp
                    );
                    // THIS CALL IS THE MOST LIKELY TO CRASH
                    multi_timeframe_manager->process_new_second_bar(mtf_bar);
                    } catch (...) {
                    // MTH-TS failed - log to stderr and continue
                    std::cerr << "BAR: MTH-TS processing failed - continuing..." << std::endl;
                }
            }
        } catch (...) {
            std::cerr << "BAR: Data mutex/processing error - continuing..." << std::endl;
        }

        return true;
    } catch (...) {
        // ABSOLUTE CATCH-ALL - NO EXCEPTIONS CAN ESCAPE THIS FUNCTION
        std::cerr << "BAR: CRITICAL - Unknown exception in process_bar_message - system must continue" << std::endl;
        return false;
    }
}

bool PolygonCryptoClient::process_quote_message(const json& msg_json) {
    // ABSOLUTE DEFENSIVE PROGRAMMING - NO EXCEPTIONS CAN ESCAPE
    try {
        // Get symbol - ABSOLUTELY SAFE
        std::string pair;
        try {
            pair = msg_json.value("pair", "");
                        } catch (...) {
            std::cerr << "QUOTE: Failed to get pair" << std::endl;
            return false;
                        }

        if (pair.empty()) {
                        return false;
        }

        // Convert symbol format - ABSOLUTELY SAFE
        std::string symbol = pair;
        try {
            size_t dash_pos = symbol.find('-');
            if (dash_pos != std::string::npos) {
                symbol.replace(dash_pos, 1, "/");
            }
        } catch (...) {
            std::cerr << "QUOTE: Symbol conversion failed" << std::endl;
                        return false;
                    }

        // Extract quote data - EACH ACCESS MUST BE SAFE
        double bid_price = 0.0;
        try {
            bid_price = msg_json.value("bp", 0.0);
                    } catch (...) {
            std::cerr << "QUOTE: Failed to get bid_price" << std::endl;
                    return false;
        }

        double ask_price = 0.0;
        try {
            ask_price = msg_json.value("ap", 0.0);
                    } catch (...) {
            std::cerr << "QUOTE: Failed to get ask_price" << std::endl;
            return false;
                    }

        // Validate data
        if (bid_price <= 0.0 || ask_price <= 0.0) {
            std::cerr << "QUOTE: Invalid prices - bid: " << bid_price << ", ask: " << ask_price << std::endl;
                    return false;
                }

        // Update data structures - ABSOLUTELY SAFE
        try {
            std::lock_guard<std::mutex> lock(data_mutex);
            try {
                latest_prices[symbol] = (bid_price + ask_price) / 2.0;
            } catch (...) {
                std::cerr << "QUOTE: Failed to update latest_prices" << std::endl;
            }

            // Process MTH-TS quote data - THIS COULD BE DANGEROUS
            if (multi_timeframe_manager) {
        try {
                    long long timestamp_val = 0;
                    try {
                        timestamp_val = msg_json.value("t", 0LL);
                    } catch (...) {
                        std::cerr << "QUOTE: Failed to get timestamp for MTH-TS" << std::endl;
                        return true; // Still return true, just skip MTH-TS
                    }

                    std::string timestamp_str;
                    try {
                        timestamp_str = std::to_string(timestamp_val);
        } catch (...) {
                        std::cerr << "QUOTE: Timestamp to string failed for MTH-TS" << std::endl;
                        return true; // Still return true, just skip MTH-TS
        }

                    multi_timeframe_manager->process_new_quote_data(bid_price, ask_price, timestamp_str);
    } catch (...) {
                    // MTH-TS quote processing failed - log to stderr and continue
                    std::cerr << "QUOTE: MTH-TS processing failed - continuing..." << std::endl;
                }
            }
        } catch (...) {
            std::cerr << "QUOTE: Data mutex/processing error - continuing..." << std::endl;
        }

        return true;
    } catch (...) {
        // ABSOLUTE CATCH-ALL - NO EXCEPTIONS CAN ESCAPE THIS FUNCTION
        std::cerr << "QUOTE: CRITICAL - Unknown exception in process_quote_message - system must continue" << std::endl;
        return false;
    }
}
std::string PolygonCryptoClient::convert_symbol_for_websocket(const std::string& symbol) const {
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

void PolygonCryptoClient::initialize_multi_timeframe_manager(const Config::SystemConfig& system_config) {
    try {
        if (!multi_timeframe_manager) {
            try {
                multi_timeframe_manager = std::make_unique<Core::MultiTimeframeManager>(system_config, *this);
            } catch (const std::exception& constructor_exception_error) {
                throw std::runtime_error("CRITICAL: Failed to create MultiTimeframeManager: " + std::string(constructor_exception_error.what()));
            } catch (...) {
                throw std::runtime_error("CRITICAL: Unknown error creating MultiTimeframeManager");
            }

            // Load historical data to populate multi-timeframe buffers
            // This allows MTH-TS to make trading decisions immediately upon startup
            if (system_config.strategy.mth_ts_enabled) {
                try {
                    multi_timeframe_manager->load_historical_data(system_config.strategy.symbol);
                } catch (const std::exception& historical_data_exception_error) {
                    throw std::runtime_error("CRITICAL: Failed to load historical data for MTH-TS: " + std::string(historical_data_exception_error.what()));
                } catch (...) {
                    throw std::runtime_error("CRITICAL: Unknown error loading historical data for MTH-TS");
                }
            }
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("CRITICAL: Failed to initialize MultiTimeframeManager: " + std::string(e.what()));
    }
}

void PolygonCryptoClient::startPollingThread() {
    if (polling_active.load()) {
        return; // Already running
    }

    polling_active.store(true);
    polling_thread = std::thread(&PolygonCryptoClient::pollingThreadFunction, this);
    // Started polling thread for reliable data collection - no logging to avoid thread context issues
}

void PolygonCryptoClient::stopPollingThread() {
    if (!polling_active.load()) {
        return;
    }

    polling_active.store(false);
    if (polling_thread.joinable()) {
        polling_thread.join();
    }
    // Stopped polling thread - no logging to avoid thread context issues
}

void PolygonCryptoClient::pollingThreadFunction() {
    // Polling thread started - no logging to avoid thread context issues
    while (polling_active.load()) {
        try {
            // Poll for recent bars every 5 minutes (only as backup for WebSocket)
            std::this_thread::sleep_for(std::chrono::minutes(5));

            if (!polling_active.load()) {
                break;
            }

            // Poll each subscribed symbol
            for (const auto& symbol : subscribed_symbols) {
                try {
                    // Get recent 1-second bars (last 2 minutes)
                    Core::BarRequest barRequest(symbol, 120, 1);
                    auto recentBars = get_recent_bars(barRequest);

                    if (!recentBars.empty()) {
                        std::lock_guard<std::mutex> lock(data_mutex);

                        // Process the most recent bar
                        const auto& latestBar = recentBars.back();

                        // Update latest data
                        latest_bars[symbol] = latestBar;
                        latest_prices[symbol] = latestBar.close_price;

                        // Add to accumulator if available
                        auto accumulatorIt = barAccumulatorMap.find(symbol);
                        if (accumulatorIt != barAccumulatorMap.end() && accumulatorIt->second) {
                            accumulatorIt->second->addBar(latestBar);
                        }

                        // Pass to MTH-TS manager
                        if (multi_timeframe_manager) {
                            Core::MultiTimeframeBar mtf_bar(
                                latestBar.open_price, latestBar.high_price, latestBar.low_price,
                                latestBar.close_price, latestBar.volume, 0.0, latestBar.timestamp
                            );
                            multi_timeframe_manager->process_new_second_bar(mtf_bar);
                        }
                    }
                } catch (const std::exception& pollException) {
                    // Log to stderr since thread logging context is not available
                    std::cerr << "Polling error for symbol " << symbol << ": " << pollException.what() << std::endl;
                }
            }
        } catch (const std::exception& threadException) {
            // Log to stderr since thread logging context is not available
            std::cerr << "Polling thread error: " << threadException.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Brief pause before retry
        }
    }

    // Polling thread stopped - no logging
}

void PolygonCryptoClient::cleanup_resources() {
    stop_realtime_feed();
    
    std::lock_guard<std::mutex> lock(data_mutex);
    latest_quotes.clear();
    latest_prices.clear();
}

} // namespace API
} // namespace AlpacaTrader
