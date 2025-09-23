#include "config_loader.hpp"
#include "../configs/system_config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

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
}

bool load_config_from_csv(SystemConfig& cfg, const std::string& csv_path) {
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
        else if (key == "api.base_url") cfg.api.base_url = value;
        else if (key == "api.data_url") cfg.api.data_url = value;
        else if (key == "api.retry_count") cfg.api.retry_count = std::stoi(value);
        else if (key == "api.timeout_seconds") cfg.api.timeout_seconds = std::stoi(value);
        else if (key == "api.enable_ssl_verification") cfg.api.enable_ssl_verification = to_bool(value);
        else if (key == "api.rate_limit_delay_ms") cfg.api.rate_limit_delay_ms = std::stoi(value);
        else if (key == "api.api_version") cfg.api.api_version = value;

        // Target
        else if (key == "target.symbol") cfg.target.symbol = value;

        // Risk (Global System Limits Only)
        else if (key == "risk.daily_max_loss") cfg.risk.daily_max_loss = std::stod(value);
        else if (key == "risk.daily_profit_target") cfg.risk.daily_profit_target = std::stod(value);
        else if (key == "risk.max_exposure_pct") cfg.risk.max_exposure_pct = std::stod(value);
        else if (key == "risk.scale_in_multiplier") cfg.risk.scale_in_multiplier = std::stod(value);
        else if (key == "risk.buying_power_usage_factor") cfg.risk.buying_power_usage_factor = std::stod(value);
        else if (key == "risk.buying_power_validation_factor") cfg.risk.buying_power_validation_factor = std::stod(value);

        // Timing (System Infrastructure Only)
        else if (key == "timing.account_data_poll_interval_sec") cfg.timing.account_poll_sec = std::stoi(value);
        else if (key == "timing.historical_bars_fetch_minutes") cfg.timing.bar_fetch_minutes = std::stoi(value);
        else if (key == "timing.historical_bars_buffer_count") cfg.timing.bar_buffer = std::stoi(value);
        else if (key == "timing.market_status_check_interval_sec") cfg.timing.market_open_check_sec = std::stoi(value);
        else if (key == "timing.market_pre_open_buffer_minutes") cfg.timing.pre_open_buffer_min = std::stoi(value);
        else if (key == "timing.market_post_close_buffer_minutes") cfg.timing.post_close_buffer_min = std::stoi(value);
        else if (key == "timing.trading_halt_sleep_minutes") cfg.timing.halt_sleep_min = std::stoi(value);
        else if (key == "timing.countdown_display_interval_sec") cfg.timing.countdown_tick_sec = std::stoi(value);
        else if (key == "timing.thread_monitor_log_interval_sec") cfg.timing.monitoring_interval_sec = std::stoi(value);

        // Session
        else if (key == "session.et_utc_offset_hours") cfg.session.et_utc_offset_hours = std::stoi(value);
        else if (key == "session.market_open_hour") cfg.session.market_open_hour = std::stoi(value);
        else if (key == "session.market_open_minute") cfg.session.market_open_minute = std::stoi(value);
        else if (key == "session.market_close_hour") cfg.session.market_close_hour = std::stoi(value);
        else if (key == "session.market_close_minute") cfg.session.market_close_minute = std::stoi(value);

        // Flags/UX
        else if (key == "flags.debug_mode") cfg.flags.debug_mode = to_bool(value);
        else if (key == "ux.log_float_chars") cfg.ux.log_float_chars = std::stoi(value);

        // Logging
        else if (key == "logging.log_file") cfg.logging.log_file = value;
    }
    return true;
}

bool load_strategy_profiles(SystemConfig& cfg, const std::string& strategy_profiles_path) {
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
        else if (key == "strategy.buy_allow_equal_close") cfg.strategy.buy_allow_equal_close = to_bool(value);
        else if (key == "strategy.buy_require_higher_high") cfg.strategy.buy_require_higher_high = to_bool(value);
        else if (key == "strategy.buy_require_higher_low") cfg.strategy.buy_require_higher_low = to_bool(value);
        else if (key == "strategy.sell_allow_equal_close") cfg.strategy.sell_allow_equal_close = to_bool(value);
        else if (key == "strategy.sell_require_lower_low") cfg.strategy.sell_require_lower_low = to_bool(value);
        else if (key == "strategy.sell_require_lower_high") cfg.strategy.sell_require_lower_high = to_bool(value);

        // Risk parameters (can be overridden per strategy)
        else if (key == "risk.risk_per_trade") cfg.risk.risk_per_trade = std::stod(value);
        else if (key == "risk.max_value_per_trade") cfg.risk.max_value_per_trade = std::stod(value);
        else if (key == "risk.allow_multiple_positions") cfg.risk.allow_multiple_positions = to_bool(value);
        else if (key == "risk.max_layers") cfg.risk.max_layers = std::stoi(value);
        else if (key == "risk.close_on_reverse") cfg.risk.close_on_reverse = to_bool(value);

        // Timing parameters (can be overridden per strategy)
        else if (key == "timing.sleep_interval_sec") cfg.timing.sleep_interval_sec = std::stoi(value);
    }
    return true;
}

int load_system_config(SystemConfig& config) {
    std::string system_config_path = std::string("config/runtime_config.csv");
    if (!load_config_from_csv(config, system_config_path)) {
        fprintf(stderr, "Failed to load config CSV from %s\n", system_config_path.c_str());
        return 1;
    }
    
    std::string strategy_path = std::string("config/strategy_profiles.csv");
    if (!load_strategy_profiles(config, strategy_path)) {
        fprintf(stderr, "Failed to load strategy profiles from %s\n", strategy_path.c_str());
        return 1;
    }
    
    return 0;
}

bool validate_config(const SystemConfig& config, std::string& errorMessage) {
    if (config.api.api_key.empty() || config.api.api_secret.empty()) {
        errorMessage = "API credentials missing (provide via CONFIG_CSV)";
        return false;
    }
    if (config.api.base_url.empty() || config.api.data_url.empty()) {
        errorMessage = "API URLs missing (provide via CONFIG_CSV)";
        return false;
    }
    if (config.target.symbol.empty()) {
        errorMessage = "Symbol is missing (provide via CONFIG_CSV)";
        return false;
    }
    if (config.logging.log_file.empty()) {
        errorMessage = "Logging path is empty (provide via CONFIG_CSV)";
        return false;
    }
    if (config.strategy.atr_period < 2) {
        errorMessage = "strategy.atr_period must be >= 2";
        return false;
    }
    if (config.strategy.rr_ratio <= 0.0) {
        errorMessage = "strategy.rr_ratio must be > 0";
        return false;
    }
    if (config.risk.risk_per_trade <= 0.0 || config.risk.risk_per_trade >= 1.0) {
        errorMessage = "risk.risk_per_trade must be between 0 and 1";
        return false;
    }
    if (config.risk.max_exposure_pct < 0.0 || config.risk.max_exposure_pct > 100.0) {
        errorMessage = "risk.max_exposure_pct must be between 0 and 100";
        return false;
    }
    if (config.timing.sleep_interval_sec <= 0 || config.timing.account_poll_sec <= 0) {
        errorMessage = "timing.* seconds must be > 0";
        return false;
    }
    return true;
}


