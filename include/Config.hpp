// Config.hpp
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include "configs/api_config.hpp"
#include "configs/target_config.hpp"
#include "configs/strategy_config.hpp"
#include "configs/risk_config.hpp"
#include "configs/timing_config.hpp"
#include "configs/session_config.hpp"
#include "configs/flags_config.hpp"
#include "configs/ux_config.hpp"
#include "configs/logging_config.hpp"

// Aggregate config
struct Config {
    ApiConfig api;
    TargetConfig target;
    StrategyConfig strategy;
    RiskConfig risk;
    TimingConfig timing;
    SessionConfig session;
    FlagsConfig flags;
    UXConfig ux;
    LoggingConfig logging;

    Config() {
        // API credentials (hardcoded as requested)
        api.api_key = "PKFV5ODSQ2G2TA5DLIIT";
        api.api_secret = "aR2YZeyldhrVigu5v0AtFOc04YlrBYnjmeEGC7Xk";
        api.base_url = "https://paper-api.alpaca.markets";
        api.data_url = "https://data.alpaca.markets";

        // Target
        target.symbol = "TSLA";

        // Strategy
        strategy.atr_period = 14;
        strategy.atr_multiplier_entry = 0.5;
        strategy.volume_multiplier = 0.5;
        strategy.rr_ratio = 3.0;
        strategy.avg_atr_multiplier = 2;

        // Risk
        risk.risk_per_trade = 0.05;
        risk.daily_max_loss = -0.2;
        risk.daily_profit_target = 0.3;
        risk.max_exposure_pct = 50.0;
        risk.allow_multiple_positions = true;
        risk.max_layers = 5;
        risk.scale_in_multiplier = 0.5;
        risk.close_on_reverse = true;

        // Timing (one per line, named)
        timing.sleep_interval_sec = 15;
        timing.account_poll_sec = 60;
        timing.bar_fetch_minutes = 120;
        timing.bar_buffer = 20;
        timing.market_open_check_sec = 30;
        timing.pre_open_buffer_min = 5;
        timing.post_close_buffer_min = 5;
        timing.halt_sleep_min = 1;
        timing.countdown_tick_sec = 1;

        // Session
        session.et_utc_offset_hours = -4;
        session.market_open_hour = 9;
        session.market_open_minute = 30;
        session.market_close_hour = 16;
        session.market_close_minute = 0;

        // Flags
        flags.debug_mode = false;

        // UX
        ux.log_float_chars = 4;

        // Logging
        logging.log_file = "trade_log.txt";
    }
};

// (Removed module-specific view structs per user request)

#endif // CONFIG_HPP
