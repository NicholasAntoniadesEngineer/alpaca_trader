#include "market_data_client.hpp"
#include "../../logging/async_logger.hpp"
#include "../../logging/logging_macros.hpp"
#include "../../utils/http_utils.hpp"
#include "../../external/json.hpp"
#include <iomanip>
#include <sstream>
#include <vector>

using json = nlohmann::json;

std::vector<Bar> MarketDataClient::get_recent_bars(const BarRequest& reqBars) const {
    std::string start = get_iso_time_minus(timing.bar_fetch_minutes);
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    std::string end = ss.str();

    LOG_THREAD_MARKET_DATA_HEADER();
    LOG_THREAD_CONTENT("Fetching live market data for " + reqBars.symbol);

    std::vector<std::string> urls;
    urls.push_back(build_bars_url(reqBars.symbol, start, end, reqBars.limit, "iex"));
    urls.push_back(build_bars_url(reqBars.symbol, start, end, reqBars.limit, "sip"));
    urls.push_back(build_bars_url(reqBars.symbol, start, end, 10, "iex")); // Daily bars
    
    std::vector<std::string> descriptions;
    descriptions.push_back("IEX FEED (FREE - 15MIN DELAYED)");
    descriptions.push_back("SIP FEED (PAID - REAL-TIME)");
    descriptions.push_back("IEX DAILY BARS (FREE - DELAYED)");

    for (size_t i = 0; i < urls.size(); ++i) {
        log_fetch_attempt(reqBars.symbol, descriptions[i]);
        
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
            std::vector<Bar> bars = parse_bars_response(response);
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
    return std::vector<Bar>();
}

std::string MarketDataClient::build_bars_url(const std::string& symbol, const std::string& start, 
                                           const std::string& end, int limit, const std::string& feed) const {
    std::string timeframe = (limit == 10) ? "1Day" : "1Min";
    return api.data_url + "/v2/stocks/" + symbol + "/bars?start=" + start + "&end=" + end + 
           "&timeframe=" + timeframe + "&limit=" + std::to_string(limit) + 
           "&adjustment=raw&feed=" + feed;
}

std::vector<Bar> MarketDataClient::parse_bars_response(const std::string& response) const {
    std::vector<Bar> bars;
    json j = json::parse(response);
    
    if (j.contains("bars") && j["bars"].is_array() && !j["bars"].empty()) {
        for (json::const_iterator it = j["bars"].begin(); it != j["bars"].end(); ++it) {
            const json& b = *it;
            if (b.contains("o") && b.contains("h") && b.contains("l") && b.contains("c") && b.contains("v") &&
                !b["o"].is_null() && !b["h"].is_null() && !b["l"].is_null() && !b["c"].is_null() && !b["v"].is_null()) {
                Bar bar;
                bar.o = b["o"].get<double>();
                bar.h = b["h"].get<double>();
                bar.l = b["l"].get<double>();
                bar.c = b["c"].get<double>();
                bar.v = b["v"].get<long long>();
                bars.push_back(bar);
            }
        }
    } else if (j.contains("message")) {
        log_message("     |   FAIL: " + j["message"].get<std::string>(), logging.log_file);
    } else {
        log_message("     |   FAIL: No bars in response", logging.log_file);
    }
    
    return bars;
}

void MarketDataClient::log_fetch_attempt(const std::string& /* symbol */, const std::string& description) const {
    LOG_THREAD_CONTENT("Trying " + description + "...");
}

void MarketDataClient::log_fetch_result(const std::string& description, bool success, size_t bar_count) const {
    if (success) {
        LOG_THREAD_CONTENT("SUCCESS: Using " + std::to_string(bar_count) + " bars from " + description);
    } else {
        LOG_THREAD_CONTENT("FAILED: " + description + " - empty response");
    }
}

void MarketDataClient::log_fetch_failure() const {
    LOG_THREAD_SEPARATOR();
    LOG_THREAD_CONTENT("ALL DATA SOURCES FAILED:");
    LOG_THREAD_SUBCONTENT("- IEX FREE FEED: Limited symbol coverage, 15-min delay");
    LOG_THREAD_SUBCONTENT("- SIP PAID FEED: Requires subscription ($100+/month)");
    LOG_THREAD_SUBCONTENT("- Market may be closed (weekend/holiday)");
    LOG_THREAD_SUBCONTENT("- Check API key permissions and account status");
    LOG_THREAD_SECTION_FOOTER();
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
    // Construct real-time quotes endpoint URL
    std::string url = api.data_url + "/v2/stocks/" + symbol + "/quotes/latest";
    
    // Make HTTP request with standard retry/timeout settings
    HttpRequest req(url, api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
    std::string response = http_get(req);
    
        // Handle empty response (network/API issues)
        if (response.empty()) {
            LOG_THREAD_CONTENT("DATA SOURCE: IEX FREE QUOTE FAILED - falling back to DELAYED bar data");
            return 0.0;
        }
    
    // Parse JSON response and extract price data
    try {
        json j = json::parse(response);
        
        // Priority 1: Use ask price (best for buy orders)
            if (j.contains("quote") && j["quote"].contains("ap") && !j["quote"]["ap"].is_null()) {
                double ask_price = j["quote"]["ap"].get<double>();
                if (ask_price > 0) {
                    log_message("        |   DATA SOURCE: IEX FREE QUOTE (ASK) - $" + std::to_string(ask_price) + " [LIMITED COVERAGE]", logging.log_file);
                    return ask_price;
                }
            }
        
        // Priority 2: Fallback to bid price (better than delayed data)
        if (j.contains("quote") && j["quote"].contains("bp") && !j["quote"]["bp"].is_null()) {
            double bid_price = j["quote"]["bp"].get<double>();
            if (bid_price > 0) {
                log_message("        |   DATA SOURCE: IEX FREE QUOTE (BID) - $" + std::to_string(bid_price) + " [LIMITED COVERAGE]", logging.log_file);
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
