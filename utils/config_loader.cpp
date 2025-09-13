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

        // Target
        else if (key == "target.symbol") cfg.target.symbol = value;

        // Strategy
        else if (key == "strategy.atr_period") cfg.strategy.atr_period = std::stoi(value);
        else if (key == "strategy.atr_multiplier_entry") cfg.strategy.atr_multiplier_entry = std::stod(value);
        else if (key == "strategy.volume_multiplier") cfg.strategy.volume_multiplier = std::stod(value);
        else if (key == "strategy.rr_ratio") cfg.strategy.rr_ratio = std::stod(value);
        else if (key == "strategy.avg_atr_multiplier") cfg.strategy.avg_atr_multiplier = std::stoi(value);

        // Risk
        else if (key == "risk.risk_per_trade") cfg.risk.risk_per_trade = std::stod(value);
        else if (key == "risk.daily_max_loss") cfg.risk.daily_max_loss = std::stod(value);
        else if (key == "risk.daily_profit_target") cfg.risk.daily_profit_target = std::stod(value);
        else if (key == "risk.max_exposure_pct") cfg.risk.max_exposure_pct = std::stod(value);
        else if (key == "risk.allow_multiple_positions") cfg.risk.allow_multiple_positions = to_bool(value);
        else if (key == "risk.max_layers") cfg.risk.max_layers = std::stoi(value);
        else if (key == "risk.scale_in_multiplier") cfg.risk.scale_in_multiplier = std::stod(value);
        else if (key == "risk.close_on_reverse") cfg.risk.close_on_reverse = to_bool(value);

        // Timing
        else if (key == "timing.sleep_interval_sec") cfg.timing.sleep_interval_sec = std::stoi(value);
        else if (key == "timing.account_poll_sec") cfg.timing.account_poll_sec = std::stoi(value);
        else if (key == "timing.bar_fetch_minutes") cfg.timing.bar_fetch_minutes = std::stoi(value);
        else if (key == "timing.bar_buffer") cfg.timing.bar_buffer = std::stoi(value);
        else if (key == "timing.market_open_check_sec") cfg.timing.market_open_check_sec = std::stoi(value);
        else if (key == "timing.pre_open_buffer_min") cfg.timing.pre_open_buffer_min = std::stoi(value);
        else if (key == "timing.post_close_buffer_min") cfg.timing.post_close_buffer_min = std::stoi(value);
        else if (key == "timing.halt_sleep_min") cfg.timing.halt_sleep_min = std::stoi(value);
        else if (key == "timing.countdown_tick_sec") cfg.timing.countdown_tick_sec = std::stoi(value);
        else if (key == "timing.monitoring_interval_sec") cfg.timing.monitoring_interval_sec = std::stoi(value);

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


