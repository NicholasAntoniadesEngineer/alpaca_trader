#include "account_manager.hpp"
#include "core/logging/logger/async_logger.hpp"
#include "core/logging/logs/account_logs.hpp"
#include "core/logging/logs/market_data_logs.hpp"
#include "json/json.hpp"
#include <stdexcept>
#include <string>
#include <chrono>
#include <cmath>

using json = nlohmann::json;

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::AccountLogs;
using AlpacaTrader::Logging::MarketDataLogs;

AccountManager::AccountManager(const AccountManagerConfig& account_manager_config, API::ApiManager& api_mgr)
    : logging(account_manager_config.logging), strategy(account_manager_config.strategy), api_manager(api_mgr),
      last_cache_time(std::chrono::steady_clock::now() - std::chrono::seconds(account_manager_config.timing.account_data_cache_duration_seconds + 1)) {}

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
            details.position_quantity = 0;
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
                    details.position_quantity = position["qty"].is_string() ? 
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
        details.position_quantity = 0;
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
        AccountInfo account_info = fetch_account_info();
    AccountSnapshot snapshot = fetch_account_snapshot();
        return std::make_pair(account_info, snapshot);
}

AccountManager::AccountInfo AccountManager::fetch_account_info() const {
    try {
        std::string account_json = api_manager.get_account_info();
        if (account_json.empty()) {
            throw std::runtime_error("Empty account response from API");
        }
        
        json account_data = json::parse(account_json);
        AccountInfo account_info_result;
        
        // Extract account information with proper error handling
        account_info_result.account_number = account_data.value("account_number", "");
        account_info_result.status = account_data.value("status", "UNKNOWN");
        account_info_result.currency = account_data.value("currency", "USD");
        account_info_result.pattern_day_trader = account_data.value("pattern_day_trader", false);
        account_info_result.trading_blocked_reason = account_data.value("trading_blocked_reason", "");
        account_info_result.transfers_blocked_reason = account_data.value("transfers_blocked_reason", "");
        account_info_result.account_blocked_reason = account_data.value("account_blocked_reason", "");
        account_info_result.created_at = account_data.value("created_at", "");
        
        // Handle numeric fields that might be strings or numbers
        auto parse_numeric = [](const json& json_data, const std::string& field_key, double default_value) -> double {
            if (!json_data.contains(field_key)) return default_value;
            if (json_data[field_key].is_string()) return std::stod(json_data[field_key].get<std::string>());
            if (json_data[field_key].is_number()) return json_data[field_key].get<double>();
            return default_value;
        };
        
        if (!account_data.contains("equity")) {
            throw std::runtime_error("Required field 'equity' missing from account data");
        }
        account_info_result.equity = parse_numeric(account_data, "equity", 0.0);
        
        if (!account_data.contains("cash")) {
            throw std::runtime_error("Required field 'cash' missing from account data");
        }
        account_info_result.cash = parse_numeric(account_data, "cash", 0.0);
        
        if (!account_data.contains("buying_power")) {
            throw std::runtime_error("Required field 'buying_power' missing from account data");
        }
        account_info_result.buying_power = parse_numeric(account_data, "buying_power", 0.0);
        
        account_info_result.last_equity = parse_numeric(account_data, "last_equity", 0.0);
        account_info_result.long_market_value = parse_numeric(account_data, "long_market_value", 0.0);
        account_info_result.short_market_value = parse_numeric(account_data, "short_market_value", 0.0);
        account_info_result.initial_margin = parse_numeric(account_data, "initial_margin", 0.0);
        account_info_result.maintenance_margin = parse_numeric(account_data, "maintenance_margin", 0.0);
        account_info_result.sma = parse_numeric(account_data, "sma", 0.0);
        account_info_result.day_trade_count = parse_numeric(account_data, "day_trade_count", 0.0);
        account_info_result.regt_buying_power = parse_numeric(account_data, "regt_buying_power", 0.0);
        account_info_result.daytrading_buying_power = parse_numeric(account_data, "daytrading_buying_power", 0.0);
        
        return account_info_result;
        
    } catch (const std::exception& e) {
        AlpacaTrader::Logging::log_message("Account info fetch failed: " + std::string(e.what()), logging.log_file);
        throw std::runtime_error("Failed to fetch account info: " + std::string(e.what()));
    }
}

void AccountManager::fetch_account_and_position_data(ProcessedData& data) const {
    MarketDataLogs::log_market_data_attempt_table("Getting position and account data", logging.log_file);
    
    SymbolRequest sr{strategy.symbol};
    data.pos_details = fetch_position_details(sr);
    data.open_orders = fetch_open_orders_count(sr);
    
    double equity = fetch_account_equity();
    if (equity <= 0.0) {
        data.exposure_pct = 0.0;
    } else {
        data.exposure_pct = (std::abs(data.pos_details.current_value) / equity) * 100.0;
    }
}

} // namespace Core
} // namespace AlpacaTrader
