// AlpacaClient.cpp
#include "api/alpaca_client.hpp"
#include "utils/async_logger.hpp"
#include "utils/http_utils.hpp"
#include "external/json.hpp"
#include <iomanip>
#include <sstream>
#include <vector>
#include <cmath>

using json = nlohmann::json;

// Implement is_core_trading_hours
bool AlpacaClient::is_core_trading_hours() const {
    HttpRequest req(api.base_url + "/v2/clock", api.api_key, api.api_secret, logging.log_file, 3);
    std::string response = http_get(req);
    if (response.empty()) return false;
    try {
        json j = json::parse(response);
        if (!j.contains("is_open") || j["is_open"].is_null()) return false;
        if (!j["is_open"]) return false;
        if (!j.contains("timestamp") || j["timestamp"].is_null()) return false;
        std::string timestamp = j["timestamp"];
        std::tm t = {};
        std::istringstream ss(timestamp);
        ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S");
        int hour = t.tm_hour + session.et_utc_offset_hours;
        int min = t.tm_min;
        int open_h = session.market_open_hour;
        int open_m = session.market_open_minute;
        int close_h = session.market_close_hour;
        int close_m = session.market_close_minute;
        bool after_open = (hour > open_h) || (hour == open_h && min >= open_m);
        bool before_close = (hour < close_h) || (hour == close_h && min <= close_m);
        return after_open && before_close;
    } catch (...) {
        log_message("Error parsing clock response: " + response, logging.log_file);
    }
    return false;
}

bool AlpacaClient::is_within_fetch_window() const {
    // Allow data fetching only when: market is open OR within N minutes before next open
    HttpRequest req(api.base_url + "/v2/clock", api.api_key, api.api_secret, logging.log_file, 3);
    std::string response = http_get(req);
    if (response.empty()) return false;
    try {
        json j = json::parse(response);
        if (j.contains("is_open") && j["is_open"].is_boolean() && j["is_open"].get<bool>()) {
            return true;
        }
        if (j.contains("timestamp") && !j["timestamp"].is_null() && j.contains("next_open") && !j["next_open"].is_null()) {
            std::string now_s = j["timestamp"].get<std::string>();
            std::string next_open_s = j["next_open"].get<std::string>();
            std::tm now_tm = {}, open_tm = {};
            {
                std::istringstream ss(now_s);
                ss >> std::get_time(&now_tm, "%Y-%m-%dT%H:%M:%S");
            }
            {
                std::istringstream ss(next_open_s);
                ss >> std::get_time(&open_tm, "%Y-%m-%dT%H:%M:%S");
            }
            // Convert to minutes since epoch (approx; ignore TZ suffix fractional seconds)
            auto to_minutes = [](const std::tm& t) {
                std::tm tmp = t;
                return static_cast<long long>(std::mktime(&tmp) / 60);
            };
            long long now_min = to_minutes(now_tm);
            long long open_min = to_minutes(open_tm);
            if (now_min > 0 && open_min > 0) {
                long long mins_to_open = open_min - now_min;
                return mins_to_open >= 0 && mins_to_open <= timing.pre_open_buffer_min;
            }
        }
    } catch (...) {
        log_message("Error parsing clock response for fetch window: " + response, logging.log_file);
    }
    return false;
}

std::vector<Bar> AlpacaClient::get_recent_bars(const BarRequest& reqBars) const {
    std::string start = get_iso_time_minus(timing.bar_fetch_minutes);
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    std::string end = ss.str();

    log_message("   ┌─ ", logging.log_file);
    log_message("   │ Fetching live market data for " + reqBars.symbol, logging.log_file);

    std::vector<std::string> urls = {
        api.data_url + "/v2/stocks/" + reqBars.symbol + "/bars?start=" + start + "&end=" + end + "&timeframe=1Min&limit=" + std::to_string(reqBars.limit) + "&adjustment=raw&feed=iex",
        api.data_url + "/v2/stocks/" + reqBars.symbol + "/bars?start=" + start + "&end=" + end + "&timeframe=1Min&limit=" + std::to_string(reqBars.limit) + "&adjustment=raw&feed=sip",
        api.data_url + "/v2/stocks/" + reqBars.symbol + "/bars?start=" + start + "&end=" + end + "&timeframe=1Day&limit=10&adjustment=raw&feed=iex"
    };
    std::vector<std::string> descriptions = {"IEX feed (free)", "SIP feed (paid)", "Daily bars"};

    std::vector<Bar> bars;
    for (size_t i = 0; i < urls.size(); ++i) {
        log_message("   │ Trying " + descriptions[i] + "...", logging.log_file);
        HttpRequest req(urls[i], api.api_key, api.api_secret, logging.log_file, 3);
        std::string response = http_get(req);
        if (response.empty()) {
            log_message("   │ FAIL: Empty response", logging.log_file);
            continue;
        }
        if (response.find("subscription does not permit") != std::string::npos) {
            log_message("   │ FAIL: Subscription required", logging.log_file);
            continue;
        }
        try {
            json j = json::parse(response);
            if (j.contains("bars") && j["bars"].is_array() && !j["bars"].empty()) {
                for (const auto& b : j["bars"]) {
                    if (b.contains("o") && b.contains("h") && b.contains("l") && b.contains("c") && b.contains("v") &&
                        !b["o"].is_null() && !b["h"].is_null() && !b["l"].is_null() && !b["c"].is_null() && !b["v"].is_null()) {
                        bars.push_back({b["o"].get<double>(), b["h"].get<double>(), b["l"].get<double>(), b["c"].get<double>(), b["v"].get<long long>()});
                    }
                }
                if (!bars.empty()) {
                    log_message("   │ PASS: Retrieved " + std::to_string(bars.size()) + " bars from " + descriptions[i], logging.log_file);
                    log_message("   └─ ", logging.log_file);
                    return bars;
                }
            } else if (j.contains("message")) {
                log_message("   │ FAIL: " + j["message"].get<std::string>(), logging.log_file);
            } else {
                log_message("   │ FAIL: No bars in response", logging.log_file);
            }
        } catch (const std::exception& e) {
            log_message("   │ FAIL: Parse error: " + std::string(e.what()), logging.log_file);
        }
    }

    log_message("   │", logging.log_file);
    log_message("   │ FAIL: ", logging.log_file);
    log_message("   │      - Market data endpoints failed", logging.log_file);
    log_message("   │      - Market may be closed (weekend/holiday)", logging.log_file);
    log_message("   │      - Subscription may be required for selected feed", logging.log_file);
    log_message("   │      - Check API key permissions and validity", logging.log_file);
    log_message("   └─ ", logging.log_file);
    return bars;
}

void AlpacaClient::place_bracket_order(const OrderRequest& oreq) const {
    if (oreq.qty <= 0) return;
    json order = {
        {"symbol", target.symbol},
        {"qty", std::to_string(oreq.qty)},
        {"side", oreq.side},
        {"type", "market"},
        {"time_in_force", "gtc"},
        {"order_class", "bracket"}
    };
    if (oreq.side == "buy") {
        order["take_profit"] = {{"limit_price", std::to_string(oreq.tp)}};
        order["stop_loss"] = {{"stop_price", std::to_string(oreq.sl)}};
    } else {
        order["take_profit"] = {{"limit_price", std::to_string(oreq.tp)}};
        order["stop_loss"] = {{"stop_price", std::to_string(oreq.sl)}};
    }
    HttpRequest req(api.base_url + "/v2/orders", api.api_key, api.api_secret, logging.log_file, 3, order.dump());
    std::string response = http_post(req);
    std::string log_msg = "Bracket order attempt: " + oreq.side + " " + std::to_string(oreq.qty) + " (TP: " + std::to_string(oreq.tp) + ", SL: " + std::to_string(oreq.sl) + ")";
    if (!response.empty()) {
        try {
            json j = json::parse(response);
            if (j.contains("id")) {
                log_msg += "   Success: ID " + j["id"].get<std::string>();
            } else {
                log_msg += "   Response: " + response;
            }
        } catch (...) {
            log_msg += "   Response: " + response;
        }
    } else {
        log_msg += "   Failed";
    }
    log_message(log_msg, logging.log_file);
}

void AlpacaClient::close_position(const ClosePositionRequest& creq) const {
    if (creq.currentQty == 0) return;
    std::string side = (creq.currentQty > 0) ? "sell" : "buy";
    int abs_qty = std::abs(creq.currentQty);
    json order = {
        {"symbol", target.symbol},
        {"qty", std::to_string(abs_qty)},
        {"side", side},
        {"type", "market"},
        {"time_in_force", "gtc"}
    };
    HttpRequest req(api.base_url + "/v2/orders", api.api_key, api.api_secret, logging.log_file, 3, order.dump());
    std::string response = http_post(req);
    std::string log_msg = "Closing position: " + side + " " + std::to_string(abs_qty);
    if (!response.empty()) {
        try {
            json j = json::parse(response);
            if (j.contains("id")) {
                log_msg += "   Success: ID " + j["id"].get<std::string>();
            } else {
                log_msg += "   Response: " + response;
            }
        } catch (...) {
            log_msg += "   Response: " + response;
        }
    } else {
        log_msg += "   Failed";
    }
    log_message(log_msg, logging.log_file);
}
