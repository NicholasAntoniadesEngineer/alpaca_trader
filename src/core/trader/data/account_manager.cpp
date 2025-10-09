#include "account_manager.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/account_logs.hpp"
#include "core/utils/http_utils.hpp"
#include "configs/api_config.hpp"
#include "json/json.hpp"
#include <cmath>

using json = nlohmann::json;

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::AccountLogs;

AccountManager::AccountManager(const AccountManagerConfig& cfg)
    : api(cfg.api), logging(cfg.logging), strategy(cfg.strategy),
      cache_duration_seconds(cfg.timing.account_data_cache_duration_seconds),
      last_cache_time(std::chrono::steady_clock::now() - std::chrono::seconds(cfg.timing.account_data_cache_duration_seconds + 1)) {}

double AccountManager::fetch_account_equity() const {
    using namespace AlpacaTrader::Config;
    HttpRequest req(api.base_url + api.endpoints.trading.account, api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
    std::string response = http_get(req);
    if (response.empty()) {
        AccountLogs::log_account_empty_response(logging.log_file);
        return 0.0;
    }
    try {
        json j = json::parse(response);
        if (j.contains("message")) {
            AccountLogs::log_account_api_error(j["message"].get<std::string>(), logging.log_file);
            return 0.0;
        }
        if (j.contains("equity") && !j["equity"].is_null()) {
            return std::stod(j["equity"].get<std::string>());
        } else {
            AccountLogs::log_account_field_missing("Equity", logging.log_file);
        }
    } catch (const std::exception& e) {
        AccountLogs::log_account_parse_error(e.what(), response, logging.log_file);
    }
    return 0.0;
}

double AccountManager::fetch_buying_power() const {
    using namespace AlpacaTrader::Config;
    HttpRequest req(api.base_url + api.endpoints.trading.account, api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
    std::string response = http_get(req);
    if (response.empty()) {
        AccountLogs::log_account_empty_response(logging.log_file);
        return 0.0;
    }
    try {
        json j = json::parse(response);
        if (j.contains("message")) {
            AccountLogs::log_account_api_error(j["message"].get<std::string>(), logging.log_file);
            return 0.0;
        }
        if (j.contains("buying_power") && !j["buying_power"].is_null()) {
            return std::stod(j["buying_power"].get<std::string>());
        } else {
            AccountLogs::log_account_field_missing("Buying power", logging.log_file);
        }
    } catch (const std::exception& e) {
        AccountLogs::log_account_parse_error(e.what(), response, logging.log_file);
    }
    return 0.0;
}

PositionDetails AccountManager::fetch_position_details(const SymbolRequest& req_sym) const {
    using namespace AlpacaTrader::Config;
    PositionDetails details;
    std::string url = api.base_url + api.endpoints.trading.position_by_symbol;
    // Replace {symbol} placeholder
    size_t pos = url.find("{symbol}");
    if (pos != std::string::npos) {
        url.replace(pos, 8, req_sym.symbol);
    }
    HttpRequest req(url, api.api_key, api.api_secret, logging.log_file, 
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
        AccountLogs::log_position_parse_error(e.what(), response, logging.log_file);
    }
    return details;
}

int AccountManager::fetch_open_orders_count(const SymbolRequest& req_sym) const {
    using namespace AlpacaTrader::Config;
    std::string url = api.base_url + api.endpoints.trading.orders_by_symbol;
    // Replace {symbol} placeholder
    size_t pos = url.find("{symbol}");
    if (pos != std::string::npos) {
        url.replace(pos, 8, req_sym.symbol);
    }
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
        AccountLogs::log_orders_parse_error(e.what(), response, logging.log_file);
        return 0;
    }
}

AccountSnapshot AccountManager::fetch_account_snapshot() const {
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    // Check if cache is still valid
    auto now = std::chrono::steady_clock::now();
    auto cache_age = std::chrono::duration_cast<std::chrono::seconds>(now - last_cache_time).count();
    
    if (cache_age < cache_duration_seconds) {
        // Return cached data
        return cached_data.second;
    }
    
    // Cache expired, fetch fresh data
    auto [account_info, snapshot] = fetch_account_data_bundled();
    
    // Update cache
    cached_data = {account_info, snapshot};
    last_cache_time = now;
    
    return snapshot;
}

// Optimized method to get both account info and snapshot with minimal API calls
std::pair<AccountManager::AccountInfo, AccountSnapshot> AccountManager::fetch_account_data_bundled() const {
    AccountInfo info = {};
    AccountSnapshot snapshot;
    
    // Single request to get all account data
    using namespace AlpacaTrader::Config;
    HttpRequest req(api.base_url + api.endpoints.trading.account, api.api_key, api.api_secret, logging.log_file, 
                   api.retry_count, api.timeout_seconds, api.enable_ssl_verification, api.rate_limit_delay_ms);
    std::string response = http_get(req);
    
    if (response.empty()) {
        AccountLogs::log_account_empty_response(logging.log_file);
        return {info, snapshot};
    }
    
    try {
        json account = json::parse(response);
        if (account.contains("message")) {
            AccountLogs::log_account_api_error(account["message"].get<std::string>(), logging.log_file);
            return {info, snapshot};
        }
        
        // Extract all account fields for both info and snapshot
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
            double equity = std::stod(account["equity"].get<std::string>());
            info.equity = equity;
            snapshot.equity = equity;  // Use same value for snapshot
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
        
        // Get position details and orders (these are different endpoints)
        SymbolRequest sreq{strategy.symbol};
        snapshot.pos_details = fetch_position_details(sreq);
        snapshot.open_orders = fetch_open_orders_count(sreq);
        snapshot.exposure_pct = (snapshot.equity > 0.0) ? (std::abs(snapshot.pos_details.current_value) / snapshot.equity) * 100.0 : 0.0;
        
    } catch (const std::exception& e) {
        AccountLogs::log_account_parse_error(e.what(), response, logging.log_file);
    }
    
    return {info, snapshot};
}

AccountManager::AccountInfo AccountManager::fetch_account_info() const {
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    // Check if cache is still valid
    auto now = std::chrono::steady_clock::now();
    auto cache_age = std::chrono::duration_cast<std::chrono::seconds>(now - last_cache_time).count();
    
    if (cache_age < cache_duration_seconds) {
        // Return cached data
        return cached_data.first;
    }
    
    // Cache expired, fetch fresh data
    auto [account_info, snapshot] = fetch_account_data_bundled();
    
    // Update cache
    cached_data = {account_info, snapshot};
    last_cache_time = now;
    
    return account_info;
}

} // namespace Core
} // namespace AlpacaTrader
