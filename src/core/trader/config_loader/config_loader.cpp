#include "config_loader.hpp"
#include "configs/system_config.hpp"
#include "configs/thread_config.hpp"
#include "core/logging/logging_macros.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

using AlpacaTrader::Logging::log_message;

namespace {
    inline std::string trim(const std::string& s) {
        const char* ws = " \t\r\n";
        auto b = s.find_first_not_of(ws);
        auto e = s.find_last_not_of(ws);
        if (b == std::string::npos) return "";
        return s.substr(b, e - b + 1);
    }

    inline bool to_bool(const std::string& v) {
        std::string s = v; std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s == "1" || s == "true" || s == "yes";
    }

    inline AlpacaTrader::Config::Priority string_to_priority(const std::string& str) {
        if (str == "REALTIME") return AlpacaTrader::Config::Priority::REALTIME;
        if (str == "HIGHEST")  return AlpacaTrader::Config::Priority::HIGHEST;
        if (str == "HIGH")     return AlpacaTrader::Config::Priority::HIGH;
        if (str == "NORMAL")   return AlpacaTrader::Config::Priority::NORMAL;
        if (str == "LOW")      return AlpacaTrader::Config::Priority::LOW;
        if (str == "LOWEST")   return AlpacaTrader::Config::Priority::LOWEST;
        return AlpacaTrader::Config::Priority::NORMAL; // Default fallback
    }
}

bool load_config_from_csv(AlpacaTrader::Config::SystemConfig& cfg, const std::string& csv_path) {
    std::ifstream in(csv_path);
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string key, value;
        if (!std::getline(ss, key, ',')) continue;
        if (!std::getline(ss, value)) continue;
        key = trim(key); value = trim(value);

        // API
        if (key == "api.api_key") cfg.api.api_key = value;
        else if (key == "api.api_secret") cfg.api.api_secret = value;
        else if (key == "api.base_url") {
            cfg.api.base_url = value;
        }
        else if (key == "api.data_url") cfg.api.data_url = value;
        else if (key == "api.retry_count") cfg.api.retry_count = std::stoi(value);
        else if (key == "api.timeout_seconds") cfg.api.timeout_seconds = std::stoi(value);
        else if (key == "api.enable_ssl_verification") cfg.api.enable_ssl_verification = to_bool(value);
        else if (key == "api.rate_limit_delay_ms") cfg.api.rate_limit_delay_ms = std::stoi(value);
        else if (key == "api.api_version") cfg.api.api_version = value;

        // Endpoints Configuration (only used endpoints)
        else if (key == "endpoints.trading_paper_url") cfg.api.endpoints.trading_paper_url = value;
        else if (key == "endpoints.trading_live_url") cfg.api.endpoints.trading_live_url = value;
        else if (key == "endpoints.market_data_url") cfg.api.endpoints.market_data_url = value;
        else if (key == "endpoints.api_version") cfg.api.endpoints.api_version = value;
        
        // Trading Endpoints (used)
        else if (key == "endpoints.trading.account") cfg.api.endpoints.trading.account = value;
        else if (key == "endpoints.trading.positions") cfg.api.endpoints.trading.positions = value;
        else if (key == "endpoints.trading.position_by_symbol") cfg.api.endpoints.trading.position_by_symbol = value;
        else if (key == "endpoints.trading.orders") cfg.api.endpoints.trading.orders = value;
        else if (key == "endpoints.trading.orders_by_symbol") cfg.api.endpoints.trading.orders_by_symbol = value;
        else if (key == "endpoints.trading.clock") cfg.api.endpoints.trading.clock = value;
        
        // Market Data Endpoints (used)
        else if (key == "endpoints.market_data.bars") cfg.api.endpoints.market_data.bars = value;
        else if (key == "endpoints.market_data.quotes_latest") cfg.api.endpoints.market_data.quotes_latest = value;

        // Target
        else if (key == "target.symbol") cfg.target.symbol = value;

        // Risk (Global System Limits Only)
        else if (key == "risk.daily_max_loss") cfg.risk.daily_max_loss = std::stod(value);
        else if (key == "risk.daily_profit_target") cfg.risk.daily_profit_target = std::stod(value);
        else if (key == "risk.max_exposure_pct") cfg.risk.max_exposure_pct = std::stod(value);
        else if (key == "risk.scale_in_multiplier") cfg.risk.scale_in_multiplier = std::stod(value);
        else if (key == "risk.buying_power_usage_factor") cfg.risk.buying_power_usage_factor = std::stod(value);
        else if (key == "risk.buying_power_validation_factor") cfg.risk.buying_power_validation_factor = std::stod(value);

        // Thread polling intervals
        else if (key == "timing.thread_market_data_poll_interval_sec") cfg.timing.thread_market_data_poll_interval_sec = std::stoi(value);
        else if (key == "timing.thread_account_data_poll_interval_sec") cfg.timing.thread_account_data_poll_interval_sec = std::stoi(value);
        else if (key == "timing.thread_market_gate_poll_interval_sec") cfg.timing.thread_market_gate_poll_interval_sec = std::stoi(value);
        else if (key == "timing.thread_trader_poll_interval_sec") cfg.timing.thread_trader_poll_interval_sec = std::stoi(value);
        else if (key == "timing.thread_logging_poll_interval_sec") cfg.timing.thread_logging_poll_interval_sec = std::stoi(value);
        
        // Data configuration
        else if (key == "timing.historical_bars_fetch_minutes") cfg.timing.bar_fetch_minutes = std::stoi(value);
        else if (key == "timing.historical_bars_buffer_count") cfg.timing.bar_buffer = std::stoi(value);
        else if (key == "timing.account_data_cache_duration_sec") cfg.timing.account_data_cache_duration_sec = std::stoi(value);
        else if (key == "timing.market_data_max_age_seconds") cfg.timing.market_data_max_age_seconds = std::stoi(value);
        else if (key == "timing.market_pre_open_buffer_minutes") cfg.timing.pre_open_buffer_min = std::stoi(value);
        else if (key == "timing.market_post_close_buffer_minutes") cfg.timing.post_close_buffer_min = std::stoi(value);
        else if (key == "timing.market_close_buffer_minutes") cfg.timing.market_close_buffer_min = std::stoi(value);
        else if (key == "timing.trading_halt_sleep_minutes") cfg.timing.halt_sleep_min = std::stoi(value);
        else if (key == "timing.countdown_display_interval_sec") cfg.timing.countdown_tick_sec = std::stoi(value);
        else if (key == "timing.enable_thread_monitoring") cfg.timing.enable_thread_monitoring = (value == "true");
        else if (key == "timing.thread_monitor_log_interval_sec") cfg.timing.monitoring_interval_sec = std::stoi(value);
        
        // Thread initialization delays
        else if (key == "timing.thread_initialization_delay_ms") cfg.timing.thread_initialization_delay_ms = std::stoi(value);
        else if (key == "timing.thread_startup_delay_ms") cfg.timing.thread_startup_delay_ms = std::stoi(value);
        
        // Order cancellation timing
        else if (key == "timing.order_cancellation_wait_ms") cfg.timing.order_cancellation_wait_ms = std::stoi(value);
        else if (key == "timing.position_verification_wait_ms") cfg.timing.position_verification_wait_ms = std::stoi(value);
        else if (key == "timing.position_settlement_wait_ms") cfg.timing.position_settlement_wait_ms = std::stoi(value);
        else if (key == "timing.max_concurrent_cancellations") cfg.timing.max_concurrent_cancellations = std::stoi(value);
        else if (key == "timing.min_order_interval_sec") cfg.timing.min_order_interval_sec = std::stoi(value);
        else if (key == "timing.enable_wash_trade_prevention") cfg.timing.enable_wash_trade_prevention = to_bool(value);
        
        // Timing precision configuration
        else if (key == "timing.cpu_usage_precision") cfg.timing.cpu_usage_precision = std::stoi(value);
        else if (key == "timing.rate_precision") cfg.timing.rate_precision = std::stoi(value);

        // Session
        else if (key == "session.et_utc_offset_hours") cfg.session.et_utc_offset_hours = std::stoi(value);
        else if (key == "session.market_open_hour") cfg.session.market_open_hour = std::stoi(value);
        else if (key == "session.market_open_minute") cfg.session.market_open_minute = std::stoi(value);
        else if (key == "session.market_close_hour") cfg.session.market_close_hour = std::stoi(value);
        else if (key == "session.market_close_minute") cfg.session.market_close_minute = std::stoi(value);

        // Orders
        else if (key == "orders.min_quantity") cfg.orders.min_quantity = std::stoi(value);
        else if (key == "orders.price_precision") cfg.orders.price_precision = std::stoi(value);
        else if (key == "orders.default_time_in_force") cfg.orders.default_time_in_force = value;
        else if (key == "orders.default_order_type") cfg.orders.default_order_type = value;
        else if (key == "orders.position_closure_side_buy") cfg.orders.position_closure_side_buy = value;
        else if (key == "orders.position_closure_side_sell") cfg.orders.position_closure_side_sell = value;
        else if (key == "orders.orders_endpoint") {
            cfg.orders.orders_endpoint = value;
        }
        else if (key == "orders.positions_endpoint") cfg.orders.positions_endpoint = value;
        else if (key == "orders.orders_status_filter") cfg.orders.orders_status_filter = value;
        else if (key == "orders.zero_quantity_check") cfg.orders.zero_quantity_check = to_bool(value);
        else if (key == "orders.position_verification_enabled") cfg.orders.position_verification_enabled = to_bool(value);
        else if (key == "orders.cancellation_mode") cfg.orders.cancellation_mode = value;
        else if (key == "orders.cancel_opposite_side") cfg.orders.cancel_opposite_side = to_bool(value);
        else if (key == "orders.cancel_same_side") cfg.orders.cancel_same_side = to_bool(value);
        else if (key == "orders.max_orders_to_cancel") cfg.orders.max_orders_to_cancel = std::stoi(value);

        // Logging
        else if (key == "logging.log_file") cfg.logging.log_file = value;
    }
    return true;
}

bool load_strategy_profiles(AlpacaTrader::Config::SystemConfig& cfg, const std::string& strategy_profiles_path) {
    std::ifstream in(strategy_profiles_path);
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue; // Skip comments and empty lines
        std::stringstream ss(line);
        std::string key, value;
        if (!std::getline(ss, key, ',')) continue;
        if (!std::getline(ss, value)) continue;
        key = trim(key); value = trim(value);

        // Strategy parameters
        if (key == "strategy.atr_period") cfg.strategy.atr_period = std::stoi(value);
        else if (key == "strategy.atr_multiplier_entry") cfg.strategy.atr_multiplier_entry = std::stod(value);
        else if (key == "strategy.volume_multiplier") cfg.strategy.volume_multiplier = std::stod(value);
        else if (key == "strategy.rr_ratio") cfg.strategy.rr_ratio = std::stod(value);
        else if (key == "strategy.avg_atr_multiplier") cfg.strategy.avg_atr_multiplier = std::stoi(value);
        else if (key == "strategy.atr_absolute_threshold") cfg.strategy.atr_absolute_threshold = std::stod(value);
        else if (key == "strategy.use_absolute_atr_threshold") cfg.strategy.use_absolute_atr_threshold = (value == "true");
        
        // Momentum signal configuration
        else if (key == "strategy.min_price_change_pct") cfg.strategy.min_price_change_pct = std::stod(value);
        else if (key == "strategy.min_volume_change_pct") cfg.strategy.min_volume_change_pct = std::stod(value);
        else if (key == "strategy.min_volatility_pct") cfg.strategy.min_volatility_pct = std::stod(value);
        else if (key == "strategy.min_sell_volume_change_pct") cfg.strategy.min_sell_volume_change_pct = std::stod(value);
        else if (key == "strategy.min_sell_volatility_pct") cfg.strategy.min_sell_volatility_pct = std::stod(value);
        else if (key == "strategy.signal_strength_threshold") cfg.strategy.signal_strength_threshold = std::stod(value);
        
        // Signal strength weighting configuration
        else if (key == "strategy.basic_pattern_weight") cfg.strategy.basic_pattern_weight = std::stod(value);
        else if (key == "strategy.momentum_weight") cfg.strategy.momentum_weight = std::stod(value);
        else if (key == "strategy.volume_weight") cfg.strategy.volume_weight = std::stod(value);
        else if (key == "strategy.volatility_weight") cfg.strategy.volatility_weight = std::stod(value);
        
        // Doji pattern detection configuration
        else if (key == "strategy.doji_body_threshold") cfg.strategy.doji_body_threshold = std::stod(value);
        else if (key == "strategy.buy_allow_equal_close") cfg.strategy.buy_allow_equal_close = to_bool(value);
        else if (key == "strategy.buy_require_higher_high") cfg.strategy.buy_require_higher_high = to_bool(value);
        else if (key == "strategy.buy_require_higher_low") cfg.strategy.buy_require_higher_low = to_bool(value);
        else if (key == "strategy.sell_allow_equal_close") cfg.strategy.sell_allow_equal_close = to_bool(value);
        else if (key == "strategy.sell_require_lower_low") cfg.strategy.sell_require_lower_low = to_bool(value);
        else if (key == "strategy.sell_require_lower_high") cfg.strategy.sell_require_lower_high = to_bool(value);
        else if (key == "strategy.price_buffer_pct") cfg.strategy.price_buffer_pct = std::stod(value);
        else if (key == "strategy.min_price_buffer") cfg.strategy.min_price_buffer = std::stod(value);
        else if (key == "strategy.max_price_buffer") cfg.strategy.max_price_buffer = std::stod(value);
        else if (key == "strategy.stop_loss_buffer_dollars") cfg.strategy.stop_loss_buffer_dollars = std::stod(value);
        else if (key == "strategy.use_realtime_price_for_orders") cfg.strategy.use_realtime_price_for_orders = to_bool(value);
        else if (key == "strategy.profit_taking_threshold_dollars") cfg.strategy.profit_taking_threshold_dollars = std::stod(value);
        else if (key == "strategy.take_profit_percentage") cfg.strategy.take_profit_percentage = std::stod(value);
        else if (key == "strategy.use_take_profit_percentage") cfg.strategy.use_take_profit_percentage = to_bool(value);
        else if (key == "strategy.enable_fixed_shares") cfg.strategy.enable_fixed_shares = to_bool(value);
        else if (key == "strategy.enable_position_multiplier") cfg.strategy.enable_position_multiplier = to_bool(value);
        else if (key == "strategy.fixed_shares_per_trade") cfg.strategy.fixed_shares_per_trade = std::stoi(value);
        else if (key == "strategy.position_size_multiplier") cfg.strategy.position_size_multiplier = std::stod(value);
        else if (key == "strategy.max_quantity_per_trade") cfg.strategy.max_quantity_per_trade = std::stoi(value);
        else if (key == "strategy.min_price_threshold") cfg.strategy.min_price_threshold = std::stod(value);
        else if (key == "strategy.max_price_threshold") cfg.strategy.max_price_threshold = std::stod(value);
        
        // Strategy precision configuration
        else if (key == "strategy.ratio_precision") cfg.strategy.ratio_precision = std::stoi(value);
        else if (key == "strategy.factor_precision") cfg.strategy.factor_precision = std::stoi(value);
        else if (key == "strategy.atr_vol_precision") cfg.strategy.atr_vol_precision = std::stoi(value);

        // Risk parameters (can be overridden per strategy)
        else if (key == "risk.risk_per_trade") cfg.risk.risk_per_trade = std::stod(value);
        else if (key == "risk.max_value_per_trade") cfg.risk.max_value_per_trade = std::stod(value);
        else if (key == "risk.allow_multiple_positions") cfg.risk.allow_multiple_positions = to_bool(value);
        else if (key == "risk.max_layers") cfg.risk.max_layers = std::stoi(value);
        else if (key == "risk.close_on_reverse") cfg.risk.close_on_reverse = to_bool(value);
        
        // Risk precision configuration
        else if (key == "risk.percentage_precision") cfg.risk.percentage_precision = std::stoi(value);
        else if (key == "risk.exposure_precision") cfg.risk.exposure_precision = std::stoi(value);
        else if (key == "risk.factor_precision") cfg.risk.factor_precision = std::stoi(value);
        else if (key == "risk.currency_precision") cfg.risk.currency_precision = std::stoi(value);

        // Timing parameters (can be overridden per strategy)
    }
    return true;
}

bool load_thread_configs(AlpacaTrader::Config::SystemConfig& cfg, const std::string& thread_config_path) {
    std::ifstream in(thread_config_path);
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue; // Skip comments and empty lines
        std::stringstream ss(line);
        std::string key, value;
        if (!std::getline(ss, key, ',')) continue;
        if (!std::getline(ss, value)) continue;
        key = trim(key); value = trim(value);

        // Thread configurations
        if (key == "thread.main.priority") cfg.thread_registry.main.priority = string_to_priority(value);
        else if (key == "thread.main.cpu_affinity") cfg.thread_registry.main.cpu_affinity = std::stoi(value);
        else if (key == "thread.main.name") cfg.thread_registry.main.name = value;
        else if (key == "thread.main.use_cpu_affinity") cfg.thread_registry.main.use_cpu_affinity = to_bool(value);
        
        else if (key == "thread.trader_decision.priority") cfg.thread_registry.trader_decision.priority = string_to_priority(value);
        else if (key == "thread.trader_decision.cpu_affinity") cfg.thread_registry.trader_decision.cpu_affinity = std::stoi(value);
        else if (key == "thread.trader_decision.name") cfg.thread_registry.trader_decision.name = value;
        else if (key == "thread.trader_decision.use_cpu_affinity") cfg.thread_registry.trader_decision.use_cpu_affinity = to_bool(value);
        
        else if (key == "thread.market_data.priority") cfg.thread_registry.market_data.priority = string_to_priority(value);
        else if (key == "thread.market_data.cpu_affinity") cfg.thread_registry.market_data.cpu_affinity = std::stoi(value);
        else if (key == "thread.market_data.name") cfg.thread_registry.market_data.name = value;
        else if (key == "thread.market_data.use_cpu_affinity") cfg.thread_registry.market_data.use_cpu_affinity = to_bool(value);
        
        else if (key == "thread.account_data.priority") cfg.thread_registry.account_data.priority = string_to_priority(value);
        else if (key == "thread.account_data.cpu_affinity") cfg.thread_registry.account_data.cpu_affinity = std::stoi(value);
        else if (key == "thread.account_data.name") cfg.thread_registry.account_data.name = value;
        else if (key == "thread.account_data.use_cpu_affinity") cfg.thread_registry.account_data.use_cpu_affinity = to_bool(value);
        
        else if (key == "thread.market_gate.priority") cfg.thread_registry.market_gate.priority = string_to_priority(value);
        else if (key == "thread.market_gate.cpu_affinity") cfg.thread_registry.market_gate.cpu_affinity = std::stoi(value);
        else if (key == "thread.market_gate.name") cfg.thread_registry.market_gate.name = value;
        else if (key == "thread.market_gate.use_cpu_affinity") cfg.thread_registry.market_gate.use_cpu_affinity = to_bool(value);
        
        else if (key == "thread.logging.priority") cfg.thread_registry.logging.priority = string_to_priority(value);
        else if (key == "thread.logging.cpu_affinity") cfg.thread_registry.logging.cpu_affinity = std::stoi(value);
        else if (key == "thread.logging.name") cfg.thread_registry.logging.name = value;
        else if (key == "thread.logging.use_cpu_affinity") cfg.thread_registry.logging.use_cpu_affinity = to_bool(value);
    }
    return true;
}

int load_system_config(AlpacaTrader::Config::SystemConfig& config) {
    // Load configuration from separate logical files
    std::vector<std::string> config_files = {
        "config/api_config.csv",
        "config/endpoints_config.csv",
        "config/target_config.csv", 
        "config/strategy_config.csv",
        "config/risk_config.csv",
        "config/timing_config.csv",
        "config/session_config.csv",
        "config/logging_config.csv",
        "config/orders_config.csv"
    };
    
    // Load each configuration file
    for (const auto& config_path : config_files) {
        if (!load_config_from_csv(config, config_path)) {
            log_message("ERROR: Failed to load config CSV from " + config_path, "");
            return 1;
        }
    }
    
    // Load strategy profiles
    std::string strategy_path = std::string("config/strategy_profiles.csv");
    if (!load_strategy_profiles(config, strategy_path)) {
        log_message("ERROR: Failed to load strategy profiles from " + strategy_path, "");
        return 1;
    }
    
    // Load thread configurations
    std::string thread_config_path = std::string("config/thread_config.csv");
    if (!load_thread_configs(config, thread_config_path)) {
        log_message("ERROR: Failed to load thread configurations from " + thread_config_path, "");
        return 1;
    }
    
    return 0;
}

bool validate_config(const AlpacaTrader::Config::SystemConfig& config, std::string& error_message) {
    if (config.api.api_key.empty() || config.api.api_secret.empty()) {
        error_message = "API credentials missing (provide via CONFIG_CSV)";
        return false;
    }
    if (config.api.base_url.empty() || config.api.data_url.empty()) {
        error_message = "API URLs missing (provide via CONFIG_CSV)";
        return false;
    }
    if (config.target.symbol.empty()) {
        error_message = "Symbol is missing (provide via CONFIG_CSV)";
        return false;
    }
    if (config.logging.log_file.empty()) {
        error_message = "Logging path is empty (provide via CONFIG_CSV)";
        return false;
    }
    if (config.strategy.atr_period < 2) {
        error_message = "strategy.atr_period must be >= 2";
        return false;
    }
    if (config.strategy.rr_ratio <= 0.0) {
        error_message = "strategy.rr_ratio must be > 0";
        return false;
    }
    
    // Validate take profit configuration
    if (config.strategy.take_profit_percentage < 0.0 || config.strategy.take_profit_percentage > 1.0) {
        error_message = "strategy.take_profit_percentage must be between 0.0 and 1.0 (0% to 100%)";
        return false;
    }
    
    // Protection: Ensure only one take profit method is enabled
    if (config.strategy.use_take_profit_percentage && config.strategy.rr_ratio > 0.0) {
        // If percentage is enabled, disable risk/reward ratio for take profit
        // (risk/reward can still be used for stop loss calculation)
        AlpacaTrader::Logging::log_message("INFO: Take profit percentage enabled - using percentage-based take profit instead of risk/reward ratio", config.logging.log_file);
    }
    if (config.risk.risk_per_trade <= 0.0 || config.risk.risk_per_trade >= 1.0) {
        error_message = "risk.risk_per_trade must be between 0 and 1";
        return false;
    }
    if (config.risk.max_exposure_pct < 0.0 || config.risk.max_exposure_pct > 100.0) {
        error_message = "risk.max_exposure_pct must be between 0 and 100";
        return false;
    }
    if (config.timing.thread_market_data_poll_interval_sec <= 0 || config.timing.thread_account_data_poll_interval_sec <= 0) {
        error_message = "timing.* seconds must be > 0";
        return false;
    }
    return true;
}


