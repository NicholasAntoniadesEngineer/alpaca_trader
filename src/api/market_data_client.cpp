#include "market_data_client.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/trading_logs.hpp"
#include "core/logging/market_data_logs.hpp"
#include "core/logging/logging_macros.hpp"
#include "core/utils/http_utils.hpp"
#include "core/utils/time_utils.hpp"
#include "configs/api_config.hpp"
#include "json/json.hpp"
#include <iomanip>
#include <sstream>
#include <vector>
#include <cmath>

using json = nlohmann::json;

namespace AlpacaTrader {
namespace API {

// Using declarations for cleaner code
using AlpacaTrader::Logging::log_message;
using AlpacaTrader::Logging::TradingLogs;
using AlpacaTrader::Logging::MarketDataLogs;

std::vector<Core::Bar> MarketDataClient::get_recent_bars(const Core::BarRequest& req_bars) const {
    MarketDataLogs::log_market_data_fetch_table(req_bars.symbol, logging.log_file);

    std::vector<std::string> urls;
    std::vector<std::string> descriptions;

    if (strategy.is_crypto_asset) {
        // For crypto assets, use only the crypto endpoint
        urls.push_back(build_crypto_bars_url(req_bars.symbol, req_bars.limit));
        descriptions.push_back("CRYPTO DATA FEED (REAL-TIME)");
    } else {
        // For stocks, use multiple data sources
        urls.push_back(build_bars_url(req_bars.symbol, req_bars.limit, "iex"));
        urls.push_back(build_bars_url(req_bars.symbol, req_bars.limit, "sip"));
        urls.push_back(build_bars_url(req_bars.symbol, strategy.daily_bars_count, "iex")); // Daily bars

        descriptions.push_back("IEX FEED (FREE - 15MIN DELAYED)");
        descriptions.push_back("SIP FEED (PAID - REAL-TIME)");
        descriptions.push_back("IEX DAILY BARS (FREE - DELAYED)");
    }

    for (size_t i = 0; i < urls.size(); ++i) {
        HttpRequest req(urls[i], api.api_key, api.api_secret, logging.log_file, 
                       api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
        std::string response = http_get(req);
        
        if (response.empty()) {
            log_fetch_result(descriptions[i], false);
            continue;
        }
        
        if (response.find("subscription does not permit") != std::string::npos) {
            log_fetch_result(descriptions[i], false);
            continue;
        }
        
        try {
            std::vector<Core::Bar> bars = parse_bars_response(response);
            if (!bars.empty()) {
                log_fetch_result(descriptions[i], true, bars.size());
                LOG_THREAD_SECTION_FOOTER();
                return bars;
            } else {
                log_fetch_result(descriptions[i], false);
            }
        } catch (const std::exception& e) {
            log_message("     |   FAIL: Parse error: " + std::string(e.what()), logging.log_file);
        }
    }

    log_fetch_failure();
    return std::vector<Core::Bar>();
}

std::string MarketDataClient::build_crypto_bars_url(const std::string& symbol, int limit) const {
    using namespace AlpacaTrader::Config;

    // Determine timeframe based on limit and configuration
    std::string timeframe;
    if (limit == strategy.daily_bars_count) {
        // Use configurable daily bars timeframe for historical comparison
        timeframe = strategy.daily_bars_timeframe;
    } else {
        // Use configurable minutes per bar for intraday data
        timeframe = std::to_string(strategy.minutes_per_bar) + "Min";
    }

    // For crypto, use the configured crypto bars endpoint with placeholder replacement
    std::string endpoint = api.endpoints.crypto.bars;
    std::string url = api.market_data_url + endpoint;

    // Replace {symbol} and {timeframe} placeholders for crypto and add limit parameter
    std::string final_url = replace_url_placeholders(url, symbol, timeframe);
    final_url = final_url + "&limit=" + std::to_string(limit);
    
    return final_url;
}

std::string MarketDataClient::build_bars_url(const std::string& symbol, int limit, const std::string& feed) const {
    using namespace AlpacaTrader::Config;

    // Determine timeframe based on limit and configuration
    std::string timeframe;
    if (limit == strategy.daily_bars_count) {
        // Use configurable daily bars timeframe for historical comparison
        timeframe = strategy.daily_bars_timeframe;
    } else {
        // Use configurable minutes per bar for intraday data
        timeframe = std::to_string(strategy.minutes_per_bar) + "Min";
    }

    // For stocks, use the v2 endpoint format with symbol replacement
    std::string endpoint = api.endpoints.market_data.bars;
    std::string url = api.market_data_url + endpoint;
    // Replace {symbol} placeholder for stocks and add query parameters
    std::string final_url = replace_url_placeholders(url, symbol, timeframe);
    return final_url + "&limit=" + std::to_string(limit) +
           "&adjustment=raw&feed=" + feed;
}

std::string MarketDataClient::replace_url_placeholders(const std::string& url, const std::string& symbol, const std::string& timeframe) const {
    std::string result = url;

    // Replace {symbol} placeholder
    size_t symbol_pos = result.find("{symbol}");
    if (symbol_pos != std::string::npos) {
        result.replace(symbol_pos, 8, symbol);
    }

    // Replace {timeframe} placeholder
    size_t timeframe_pos = result.find("{timeframe}");
    if (timeframe_pos != std::string::npos) {
        result.replace(timeframe_pos, 11, timeframe);
    }

    return result;
}

std::vector<Core::Bar> MarketDataClient::parse_bars_response(const std::string& response) const {
    std::vector<Core::Bar> bars;

    if (response.empty()) {
        log_message("     |   FAIL: Empty response in parse_bars_response", logging.log_file);
        return bars;
    }

    try {
        json j = json::parse(response);

        // Handle different response formats for stocks vs crypto
        json bars_array;
        if (j.contains("bars")) {
            if (j["bars"].is_array()) {
                // Stock format: {"bars": [...]}
                bars_array = j["bars"];
            } else if (j["bars"].is_object()) {
                // Crypto format: {"bars": {"BTC/USD": [...]}}
                // Get the first (and typically only) symbol's bars
                for (auto& [symbol, symbol_bars] : j["bars"].items()) {
                    if (symbol_bars.is_array()) {
                        bars_array = symbol_bars;
                        break;
                    }
                }
            }
        }

        if (!bars_array.empty() && bars_array.is_array()) {
            for (json::const_iterator it = bars_array.begin(); it != bars_array.end(); ++it) {
                const json& b = *it;

                // Validate that all required fields exist and are not null
                if (b.contains("o") && b.contains("h") && b.contains("l") && b.contains("c") && b.contains("v") &&
                    !b["o"].is_null() && !b["h"].is_null() && !b["l"].is_null() && !b["c"].is_null() && !b["v"].is_null()) {

                    try {
                        Core::Bar bar;
                        bar.o = b["o"].get<double>();
                        bar.h = b["h"].get<double>();
                        bar.l = b["l"].get<double>();
                        bar.c = b["c"].get<double>();
                        bar.v = b["v"].get<double>();
                        
                        // Extract timestamp if available
                        if (b.contains("t") && !b["t"].is_null()) {
                            bar.t = b["t"].get<std::string>();
                        } else {
                            bar.t = ""; // Empty timestamp if not available
                        }

                        // Validate price data is reasonable (not NaN, not negative for prices)
                        if (std::isfinite(bar.o) && std::isfinite(bar.h) && std::isfinite(bar.l) && std::isfinite(bar.c) &&
                            bar.o >= 0.0 && bar.h >= 0.0 && bar.l >= 0.0 && bar.c >= 0.0 && bar.v >= 0) {
                            bars.push_back(bar);
                        } else {
                            log_message("     |   WARNING: Invalid price/volume data in bar, skipping", logging.log_file);
                        }
                    } catch (const std::exception& e) {
                        log_message("     |   WARNING: Failed to parse bar data: " + std::string(e.what()), logging.log_file);
                    }
                }
            }
        } else if (j.contains("message")) {
            std::string message = j["message"].is_string() ? j["message"].get<std::string>() : "Unknown error";
            log_message("     |   FAIL: " + message, logging.log_file);
        } else {
            log_message("     |   FAIL: No bars in response or invalid response format", logging.log_file);
        }
    } catch (const json::exception& e) {
        log_message("     |   FAIL: JSON parse error: " + std::string(e.what()), logging.log_file);
    } catch (const std::exception& e) {
        log_message("     |   FAIL: Unexpected error in parse_bars_response: " + std::string(e.what()), logging.log_file);
    }

    return bars;
}


void MarketDataClient::log_fetch_result(const std::string& description, bool success, size_t bar_count) const {
    TradingLogs::log_market_data_result_table(description, success, bar_count);
}

void MarketDataClient::log_fetch_failure() const {
    MarketDataLogs::log_market_data_failure_summary(
        "UNKNOWN", 
        "API Error", 
        "All data sources failed to provide market data", 
        0, 
        logging.log_file
    );
}

/**
 * @brief Fetches real-time current price using Alpaca's free quotes API
 * 
 * This function addresses the critical issue of using delayed data (15-min IEX delay)
 * for trading decisions. By fetching real-time quotes, we ensure exit targets
 * are calculated using current market prices, preventing order validation errors.
 * 
 * @param symbol Stock symbol to get price for
 * @return Current ask price if available, bid price as fallback, or 0.0 on failure
 * 
 * @note Priority order: ask price > bid price > failure (0.0)
 * @note Alpaca provides free real-time quotes from IEX exchange
 */
double MarketDataClient::get_current_price(const std::string& symbol) const {
    using namespace AlpacaTrader::Config;
    
    // Construct real-time quotes endpoint URL based on asset type
    std::string url;
    if (strategy.is_crypto_asset) {
        // For crypto, use the configured crypto quotes endpoint with placeholder replacement
        url = replace_url_placeholders(api.market_data_url + api.endpoints.crypto.quotes_latest, symbol, "");
    } else {
        // For stocks, use the v2 endpoint format with placeholder replacement
        std::string endpoint = api.endpoints.market_data.quotes_latest;
        url = replace_url_placeholders(api.market_data_url + endpoint, symbol, "");
    }
    
    // Make HTTP request with standard retry/timeout settings.
    HttpRequest req(url, api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
    std::string response = http_get(req);
    
        // Handle empty response (network/API issues).
        if (response.empty()) {
            LOG_THREAD_CONTENT("DATA SOURCE: IEX FREE QUOTE FAILED - falling back to DELAYED bar data");
            return 0.0;
        }
    
    // Parse JSON response and extract price data.
    try {
        json j = json::parse(response);
        
        // Priority 1: Use ask price (best for buy orders).
            if (j.contains("quote") && j["quote"].contains("ap") && !j["quote"]["ap"].is_null()) {
                double ask_price = j["quote"]["ap"].get<double>();
                if (ask_price > 0) {
                    TradingLogs::log_data_source_info_table("IEX FREE QUOTE (ASK)", ask_price, "LIMITED COVERAGE");
                    return ask_price;
                }
            }
        
        // Priority 2: Fallback to bid price (better than delayed data).
        if (j.contains("quote") && j["quote"].contains("bp") && !j["quote"]["bp"].is_null()) {
            double bid_price = j["quote"]["bp"].get<double>();
            if (bid_price > 0) {
                TradingLogs::log_data_source_info_table("IEX FREE QUOTE (BID)", bid_price, "LIMITED COVERAGE");
                return bid_price;
            }
        }
        
        // No valid price data in response
        LOG_THREAD_CONTENT("DATA SOURCE: IEX FREE QUOTE UNAVAILABLE - symbol not covered by free feed");
        
    } catch (const std::exception& e) {
        LOG_THREAD_CONTENT("DATA SOURCE: IEX FREE QUOTE PARSE ERROR - " + std::string(e.what()));
    }
    
    // Return 0.0 to indicate failure - caller should use fallback price
    return 0.0;
}

Core::QuoteData MarketDataClient::get_realtime_quotes(const std::string& symbol) const {
    using namespace AlpacaTrader::Config;
    
    Core::QuoteData quote_data;
    
    // Construct real-time quotes endpoint URL based on asset type
    std::string url;
    if (strategy.is_crypto_asset) {
        // For crypto, use the configured crypto quotes endpoint with placeholder replacement
        url = replace_url_placeholders(api.market_data_url + api.endpoints.crypto.quotes_latest, symbol, "");
        LOG_THREAD_CONTENT("DEBUG: Crypto quotes URL: " + url);
    } else {
        // For stocks, use the v2 endpoint format with placeholder replacement
        std::string endpoint = api.endpoints.market_data.quotes_latest;
        url = replace_url_placeholders(api.market_data_url + endpoint, symbol, "");
        LOG_THREAD_CONTENT("DEBUG: Stock quotes URL: " + url);
    }
    
    // Make HTTP request with standard retry/timeout settings
    HttpRequest req(url, api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
    std::string response = http_get(req);
    
    // Handle empty response (network/API issues)
    if (response.empty()) {
        LOG_THREAD_CONTENT("DATA SOURCE: Real-time quotes FAILED - empty response");
        return quote_data;
    }
    
    // Parse JSON response and extract quote data
    try {
        json j = json::parse(response);
        
        if (strategy.is_crypto_asset) {
            // Crypto format: {"quotes": {"BTC/USD": [{"ap": 108314.9, "as": 1.46611, "bp": 108069.34, "bs": 2.9235, "t": "2025-10-17T00:00:03.210003471Z"}]}}
            if (j.contains("quotes") && j["quotes"].contains(symbol) && j["quotes"][symbol].is_array() && !j["quotes"][symbol].empty()) {
                const auto& quote = j["quotes"][symbol][0];
                
                if (quote.contains("ap") && quote.contains("bp") && quote.contains("as") && quote.contains("bs") && quote.contains("t")) {
                    quote_data.ask_price = quote["ap"].get<double>();
                    quote_data.bid_price = quote["bp"].get<double>();
                    quote_data.ask_size = quote["as"].get<double>();
                    quote_data.bid_size = quote["bs"].get<double>();
                    quote_data.timestamp = quote["t"].get<std::string>();
                    quote_data.mid_price = (quote_data.ask_price + quote_data.bid_price) / 2.0;
                    
                    TradingLogs::log_data_source_info_table("CRYPTO REAL-TIME QUOTES", quote_data.mid_price, "LIVE DATA");
                }
            }
        } else {
            // Stock format: {"quote": {"ap": 662.13, "as": 2, "bp": 661.99, "bs": 2, "t": "2025-10-17T16:00:32.716168402Z"}}
            if (j.contains("quote")) {
                const auto& quote = j["quote"];
                
                if (quote.contains("ap") && quote.contains("bp") && quote.contains("as") && quote.contains("bs") && quote.contains("t")) {
                    quote_data.ask_price = quote["ap"].get<double>();
                    quote_data.bid_price = quote["bp"].get<double>();
                    quote_data.ask_size = quote["as"].get<double>();
                    quote_data.bid_size = quote["bs"].get<double>();
                    quote_data.timestamp = quote["t"].get<std::string>();
                    quote_data.mid_price = (quote_data.ask_price + quote_data.bid_price) / 2.0;
                    
                    TradingLogs::log_data_source_info_table("STOCK REAL-TIME QUOTES", quote_data.mid_price, "LIVE DATA");
                }
            }
        }
        
    } catch (const std::exception& e) {
        LOG_THREAD_CONTENT("DATA SOURCE: Real-time quotes PARSE ERROR - " + std::string(e.what()));
    }
    
    return quote_data;
}

} // namespace API
} // namespace AlpacaTrader
