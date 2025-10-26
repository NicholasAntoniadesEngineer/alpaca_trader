#include "account_manager.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/account_logs.hpp"
#include "json/json.hpp"
#include <stdexcept>
#include <string>
#include <chrono>

using json = nlohmann::json;

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::AccountLogs;

AccountManager::AccountManager(const AccountManagerConfig& cfg, API::ApiManager& api_mgr)
    : logging(cfg.logging), strategy(cfg.strategy), api_manager(api_mgr),
      last_cache_time(std::chrono::steady_clock::now() - std::chrono::seconds(cfg.timing.account_data_cache_duration_seconds + 1)) {}

double AccountManager::fetch_account_equity() const {
    try {
        // Validate API connection before attempting request
        if (!api_manager.has_provider(AlpacaTrader::Config::ApiProvider::ALPACA_TRADING)) {
            throw std::runtime_error("Alpaca trading provider not available");
        }
        
        std::string account_json = api_manager.get_account_info();
        if (account_json.empty()) {
            throw std::runtime_error("Empty account response from API - check API credentials and network connectivity");
        }
        
        json account_data = json::parse(account_json);
        
        // Check for API error responses
        if (account_data.contains("message")) {
            std::string error_message = account_data["message"].get<std::string>();
            throw std::runtime_error("API returned error: " + error_message);
        }
        
        if (account_data.contains("code")) {
            std::string error_code = account_data["code"].get<std::string>();
            throw std::runtime_error("API returned error code: " + error_code);
        }
        
        if (account_data.contains("equity") && account_data["equity"].is_string()) {
            return std::stod(account_data["equity"].get<std::string>());
        } else if (account_data.contains("equity") && account_data["equity"].is_number()) {
            return account_data["equity"].get<double>();
        }
        
        throw std::runtime_error("Account equity not found in API response - response may be malformed");
    } catch (const std::exception& e) {
        AlpacaTrader::Logging::log_message("Account equity fetch failed: " + std::string(e.what()), logging.log_file);
        throw std::runtime_error("Failed to fetch account equity: " + std::string(e.what()));
    }
}

double AccountManager::fetch_buying_power() const {
    try {
        std::string account_json = api_manager.get_account_info();
        if (account_json.empty()) {
            throw std::runtime_error("Empty account response from API");
        }
        
        json account_data = json::parse(account_json);
        if (account_data.contains("buying_power") && account_data["buying_power"].is_string()) {
            return std::stod(account_data["buying_power"].get<std::string>());
        } else if (account_data.contains("buying_power") && account_data["buying_power"].is_number()) {
            return account_data["buying_power"].get<double>();
        }
        
        throw std::runtime_error("Buying power not found in API response");
    } catch (const std::exception& e) {
        AlpacaTrader::Logging::log_message("Buying power fetch failed: " + std::string(e.what()), logging.log_file);
        throw std::runtime_error("Failed to fetch buying power: " + std::string(e.what()));
    }
}

PositionDetails AccountManager::fetch_position_details(const SymbolRequest& req_sym) const {
    try {
        std::string positions_json = api_manager.get_positions();
        if (positions_json.empty()) {
            // No positions is valid - return empty position
            PositionDetails details;
            details.qty = 0;
            details.current_value = 0.0;
            details.unrealized_pl = 0.0;
            return details;
        }
        
        json positions_data = json::parse(positions_json);
        
        // Find position for the requested symbol
        for (const auto& position : positions_data) {
            if (position.contains("symbol") && position["symbol"].get<std::string>() == req_sym.symbol) {
                PositionDetails details;
                
                if (position.contains("qty")) {
                    details.qty = position["qty"].is_string() ? 
                        std::stoi(position["qty"].get<std::string>()) : 
                        position["qty"].get<int>();
                }
                
                if (position.contains("market_value")) {
                    details.current_value = position["market_value"].is_string() ? 
                        std::stod(position["market_value"].get<std::string>()) : 
                        position["market_value"].get<double>();
                }
                
                if (position.contains("unrealized_pl")) {
                    details.unrealized_pl = position["unrealized_pl"].is_string() ? 
                        std::stod(position["unrealized_pl"].get<std::string>()) : 
                        position["unrealized_pl"].get<double>();
                }
                
                return details;
            }
        }
        
        // Position not found - return empty position
        PositionDetails details;
        details.qty = 0;
        details.current_value = 0.0;
        details.unrealized_pl = 0.0;
        return details;
        
    } catch (const std::exception& e) {
        AlpacaTrader::Logging::log_message("Position details fetch failed: " + std::string(e.what()), logging.log_file);
        throw std::runtime_error("Failed to fetch position details: " + std::string(e.what()));
    }
}

int AccountManager::fetch_open_orders_count(const SymbolRequest& req_sym) const {
    try {
        std::string orders_json = api_manager.get_open_orders();
        if (orders_json.empty()) {
            return 0; // No orders is valid
        }
        
        json orders_data = json::parse(orders_json);
        int count = 0;
        
        for (const auto& order : orders_data) {
            if (order.contains("symbol") && order["symbol"].get<std::string>() == req_sym.symbol) {
                if (order.contains("status")) {
                    std::string status = order["status"].get<std::string>();
                    if (status == "new" || status == "partially_filled" || status == "pending_new") {
                        count++;
                    }
                }
            }
        }
        
        return count;
        
    } catch (const std::exception& e) {
        AlpacaTrader::Logging::log_message("Open orders count fetch failed: " + std::string(e.what()), logging.log_file);
        throw std::runtime_error("Failed to fetch open orders count: " + std::string(e.what()));
    }
}

AccountSnapshot AccountManager::fetch_account_snapshot() const {
    AccountSnapshot snapshot;
    snapshot.equity = fetch_account_equity();
    snapshot.pos_details = fetch_position_details(SymbolRequest{strategy.symbol});
    snapshot.open_orders = fetch_open_orders_count(SymbolRequest{strategy.symbol});
    
    // Calculate exposure percentage
    if (snapshot.equity > 0.0) {
        snapshot.exposure_pct = (std::abs(snapshot.pos_details.current_value) / snapshot.equity) * 100.0;
    } else {
        snapshot.exposure_pct = 0.0;
    }
    
    return snapshot;
}

std::pair<AccountManager::AccountInfo, AccountSnapshot> AccountManager::fetch_account_data_bundled() const {
    AccountInfo info = fetch_account_info();
    AccountSnapshot snapshot = fetch_account_snapshot();
    return std::make_pair(info, snapshot);
}

AccountManager::AccountInfo AccountManager::fetch_account_info() const {
    try {
        std::string account_json = api_manager.get_account_info();
        if (account_json.empty()) {
            throw std::runtime_error("Empty account response from API");
        }
        
        json account_data = json::parse(account_json);
        AccountInfo info;
        
        // Extract account information with proper error handling
        info.account_number = account_data.value("account_number", "");
        info.status = account_data.value("status", "UNKNOWN");
        info.currency = account_data.value("currency", "USD");
        info.pattern_day_trader = account_data.value("pattern_day_trader", false);
        info.trading_blocked_reason = account_data.value("trading_blocked_reason", "");
        info.transfers_blocked_reason = account_data.value("transfers_blocked_reason", "");
        info.account_blocked_reason = account_data.value("account_blocked_reason", "");
        info.created_at = account_data.value("created_at", "");
        
        // Handle numeric fields that might be strings or numbers
        auto parse_numeric = [](const json& j, const std::string& key, double default_val = 0.0) -> double {
            if (!j.contains(key)) return default_val;
            if (j[key].is_string()) return std::stod(j[key].get<std::string>());
            if (j[key].is_number()) return j[key].get<double>();
            return default_val;
        };
        
        info.equity = parse_numeric(account_data, "equity");
        info.last_equity = parse_numeric(account_data, "last_equity");
        info.long_market_value = parse_numeric(account_data, "long_market_value");
        info.short_market_value = parse_numeric(account_data, "short_market_value");
        info.cash = parse_numeric(account_data, "cash");
        info.buying_power = parse_numeric(account_data, "buying_power");
        info.initial_margin = parse_numeric(account_data, "initial_margin");
        info.maintenance_margin = parse_numeric(account_data, "maintenance_margin");
        info.sma = parse_numeric(account_data, "sma");
        info.day_trade_count = parse_numeric(account_data, "day_trade_count");
        info.regt_buying_power = parse_numeric(account_data, "regt_buying_power");
        info.daytrading_buying_power = parse_numeric(account_data, "daytrading_buying_power");
        
        return info;
        
    } catch (const std::exception& e) {
        AlpacaTrader::Logging::log_message("Account info fetch failed: " + std::string(e.what()), logging.log_file);
        throw std::runtime_error("Failed to fetch account info: " + std::string(e.what()));
    }
}

std::string AccountManager::replace_url_placeholder(const std::string& url, const std::string& symbol) const {
    std::string result = url;
    size_t pos = result.find("{symbol}");
    if (pos != std::string::npos) {
        result.replace(pos, 8, symbol);
    }
    return result;
}

} // namespace Core
} // namespace AlpacaTrader
