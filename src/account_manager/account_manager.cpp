// AccountManager.cpp
#include "account_manager/account_manager.hpp"
#include "../logging/async_logger.hpp"
#include "../utils/http_utils.hpp"
#include "../json/json.hpp"
#include <cmath>

using json = nlohmann::json;

AccountManager::AccountManager(const AccountManagerConfig& cfg)
    : api(cfg.api), logging(cfg.logging), target(cfg.target) {}

double AccountManager::get_equity() const {
    HttpRequest req(api.base_url + "/v2/account", api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
    std::string response = http_get(req);
    if (response.empty()) {
        log_message("ERROR: Unable to retrieve account information (empty response)", logging.log_file);
        return 0.0;
    }
    try {
        json j = json::parse(response);
        if (j.contains("message")) {
            log_message("ERROR: Account API error: " + j["message"].get<std::string>(), logging.log_file);
            return 0.0;
        }
        if (j.contains("equity") && !j["equity"].is_null()) {
            return std::stod(j["equity"].get<std::string>());
        } else {
            log_message("ERROR: Equity field missing in account response", logging.log_file);
        }
    } catch (const std::exception& e) {
        log_message("ERROR: Failed to parse account data: " + std::string(e.what()) + "; raw: " + response, logging.log_file);
    }
    return 0.0;
}

double AccountManager::get_buying_power() const {
    HttpRequest req(api.base_url + "/v2/account", api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
    std::string response = http_get(req);
    if (response.empty()) {
        log_message("ERROR: Unable to retrieve account information (empty response)", logging.log_file);
        return 0.0;
    }
    try {
        json j = json::parse(response);
        if (j.contains("message")) {
            log_message("ERROR: Account API error: " + j["message"].get<std::string>(), logging.log_file);
            return 0.0;
        }
        if (j.contains("buying_power") && !j["buying_power"].is_null()) {
            return std::stod(j["buying_power"].get<std::string>());
        } else {
            log_message("ERROR: Buying power field missing in account response", logging.log_file);
        }
    } catch (const std::exception& e) {
        log_message("ERROR: Failed to parse account data: " + std::string(e.what()) + "; raw: " + response, logging.log_file);
    }
    return 0.0;
}

PositionDetails AccountManager::get_position_details(const SymbolRequest& reqSym) const {
    PositionDetails details;
    HttpRequest req(api.base_url + "/v2/positions/" + reqSym.symbol, api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
    std::string response = http_get(req);
    if (response.empty()) return details;
    try {
        json j = json::parse(response);
        if (j.contains("qty") && !j["qty"].is_null()) {
            details.qty = std::stoi(j["qty"].get<std::string>());
        }
        if (j.contains("unrealized_pl") && !j["unrealized_pl"].is_null()) {
            details.unrealized_pl = std::stod(j["unrealized_pl"].get<std::string>());
        }
        if (j.contains("market_value") && !j["market_value"].is_null()) {
            details.current_value = std::stod(j["market_value"].get<std::string>());
        }
    } catch (const std::exception& e) {
        log_message("Error parsing position details: " + std::string(e.what()), logging.log_file);
        log_message("Raw position response: " + response, logging.log_file);
    }
    return details;
}

int AccountManager::get_open_orders_count(const SymbolRequest& reqSym) const {
    std::string url = api.base_url + "/v2/orders?status=open&symbols=" + reqSym.symbol;
    HttpRequest req(url, api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
    std::string response = http_get(req);
    if (response.empty()) return 0;
    try {
        json j = json::parse(response);
        if (j.is_array()) {
            return static_cast<int>(j.size());
        }
        return 0;
    } catch (const std::exception& e) {
        log_message("Error parsing open orders: " + std::string(e.what()), logging.log_file);
        log_message("Raw orders response: " + response, logging.log_file);
        return 0;
    }
}

AccountSnapshot AccountManager::get_account_snapshot() const {
    AccountSnapshot snapshot;
    snapshot.equity = get_equity();
    SymbolRequest sreq{target.symbol};
    snapshot.pos_details = get_position_details(sreq);
    snapshot.open_orders = get_open_orders_count(sreq);
    snapshot.exposure_pct = (snapshot.equity > 0.0) ? (std::abs(snapshot.pos_details.current_value) / snapshot.equity) * 100.0 : 0.0;
    return snapshot;
}

AccountManager::AccountInfo AccountManager::get_account_info() const {
    AccountInfo info = {};  // Initialize all fields to defaults
    
    HttpRequest req(api.base_url + "/v2/account", api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
    std::string response = http_get(req);
    if (response.empty()) {
        log_message("ERROR: Could not retrieve account information", logging.log_file);
        return info;
    }
    
    try {
        json account = json::parse(response);
        if (account.contains("message")) {
            log_message("ERROR: API returned error: " + account["message"].get<std::string>(), logging.log_file);
            return info;
        }
        
        // Safely extract all account fields
        if (account.contains("account_number") && !account["account_number"].is_null()) {
            info.account_number = account["account_number"].get<std::string>();
        }
        if (account.contains("status") && !account["status"].is_null()) {
            info.status = account["status"].get<std::string>();
        }
        if (account.contains("currency") && !account["currency"].is_null()) {
            info.currency = account["currency"].get<std::string>();
        }
        if (account.contains("pattern_day_trader") && !account["pattern_day_trader"].is_null()) {
            info.pattern_day_trader = account["pattern_day_trader"].get<bool>();
        }
        if (account.contains("trading_blocked_reason") && !account["trading_blocked_reason"].is_null()) {
            info.trading_blocked_reason = account["trading_blocked_reason"].get<std::string>();
        }
        if (account.contains("transfers_blocked_reason") && !account["transfers_blocked_reason"].is_null()) {
            info.transfers_blocked_reason = account["transfers_blocked_reason"].get<std::string>();
        }
        if (account.contains("account_blocked_reason") && !account["account_blocked_reason"].is_null()) {
            info.account_blocked_reason = account["account_blocked_reason"].get<std::string>();
        }
        if (account.contains("created_at") && !account["created_at"].is_null()) {
            info.created_at = account["created_at"].get<std::string>();
        }
        if (account.contains("equity") && !account["equity"].is_null()) {
            info.equity = std::stod(account["equity"].get<std::string>());
        }
        if (account.contains("last_equity") && !account["last_equity"].is_null()) {
            info.last_equity = std::stod(account["last_equity"].get<std::string>());
        }
        if (account.contains("long_market_value") && !account["long_market_value"].is_null()) {
            info.long_market_value = std::stod(account["long_market_value"].get<std::string>());
        }
        if (account.contains("short_market_value") && !account["short_market_value"].is_null()) {
            info.short_market_value = std::stod(account["short_market_value"].get<std::string>());
        }
        if (account.contains("cash") && !account["cash"].is_null()) {
            info.cash = std::stod(account["cash"].get<std::string>());
        }
        if (account.contains("buying_power") && !account["buying_power"].is_null()) {
            info.buying_power = std::stod(account["buying_power"].get<std::string>());
        }
        if (account.contains("initial_margin") && !account["initial_margin"].is_null()) {
            info.initial_margin = std::stod(account["initial_margin"].get<std::string>());
        }
        if (account.contains("maintenance_margin") && !account["maintenance_margin"].is_null()) {
            info.maintenance_margin = std::stod(account["maintenance_margin"].get<std::string>());
        }
        if (account.contains("sma") && !account["sma"].is_null()) {
            info.sma = std::stod(account["sma"].get<std::string>());
        }
        if (account.contains("day_trade_count") && !account["day_trade_count"].is_null()) {
            info.day_trade_count = account["day_trade_count"].get<double>();
        }
        if (account.contains("regt_buying_power") && !account["regt_buying_power"].is_null()) {
            info.regt_buying_power = std::stod(account["regt_buying_power"].get<std::string>());
        }
        if (account.contains("daytrading_buying_power") && !account["daytrading_buying_power"].is_null()) {
            info.daytrading_buying_power = std::stod(account["daytrading_buying_power"].get<std::string>());
        }
        
    } catch (const std::exception& e) {
        log_message("ERROR: Failed to parse account data: " + std::string(e.what()), logging.log_file);
    }
    
    return info;
}
