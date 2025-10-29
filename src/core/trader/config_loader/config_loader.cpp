#include "config_loader.hpp"
#include "multi_api_config_loader.hpp"
#include "configs/system_config.hpp"
#include "configs/thread_config.hpp"
#include "core/logging/logger/logging_macros.hpp"
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

}

bool load_config_from_csv(AlpacaTrader::Config::SystemConfig& cfg, const std::string& csv_path) {
    // Load multi-API configuration only from api_endpoints_config.csv
    if (csv_path.find("api_endpoints_config.csv") != std::string::npos) {
        try {
            cfg.multi_api = AlpacaTrader::Core::MultiApiConfigLoader::load_from_csv(csv_path);
        } catch (const std::exception& e) {
            log_message("Failed to load multi-API configuration: " + std::string(e.what()), "");
            return false;
        }
    }
    
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

        // Trading Mode Configuration (only from strategy_config.csv)
        if (csv_path.find("strategy_config.csv") != std::string::npos) {
            if (key == "trading_mode.mode") {
                if (value.empty()) {
                    throw std::runtime_error("Trading mode is required but not provided");
                }
                cfg.trading_mode.mode = AlpacaTrader::Config::TradingModeConfig::parse_mode(value);
                // Map trading mode to strategy crypto asset indicator
                cfg.strategy.is_crypto_asset = (cfg.trading_mode.mode == AlpacaTrader::Config::TradingMode::CRYPTO);
            }
            else if (key == "trading_mode.primary_symbol") {
                if (value.empty()) {
                    throw std::runtime_error("Primary symbol is required but not provided");
                }
                cfg.trading_mode.primary_symbol = value;
                // Map primary symbol to strategy symbol
                cfg.strategy.symbol = value;
            }
        }

        // All API configuration handled by multi_api section

        // Strategy Configuration - session and other settings
        if (key == "session.et_utc_offset_hours") cfg.strategy.et_utc_offset_hours = std::stoi(value);
        else if (key == "session.market_open_hour") cfg.strategy.market_open_hour = std::stoi(value);
        else if (key == "session.market_open_minute") cfg.strategy.market_open_minute = std::stoi(value);
        else if (key == "session.market_close_hour") cfg.strategy.market_close_hour = std::stoi(value);
        else if (key == "session.market_close_minute") cfg.strategy.market_close_minute = std::stoi(value);

        // Strategy parameters
        else if (key == "strategy.atr_calculation_period") cfg.strategy.atr_calculation_period = std::stoi(value);
        else if (key == "strategy.bars_to_fetch_for_calculations") cfg.strategy.bars_to_fetch_for_calculations = std::stoi(value);
        else if (key == "strategy.minutes_per_bar") cfg.strategy.minutes_per_bar = std::stoi(value);
        else if (key == "strategy.atr_calculation_bars") cfg.strategy.atr_calculation_bars = std::stoi(value);
        else if (key == "strategy.daily_bars_timeframe") cfg.strategy.daily_bars_timeframe = value;
        else if (key == "strategy.daily_bars_count") cfg.strategy.daily_bars_count = std::stoi(value);
        else if (key == "strategy.entry_signal_atr_multiplier") cfg.strategy.entry_signal_atr_multiplier = std::stod(value);
        else if (key == "strategy.entry_signal_volume_multiplier") cfg.strategy.entry_signal_volume_multiplier = std::stod(value);
        else if (key == "strategy.crypto_volume_multiplier") cfg.strategy.crypto_volume_multiplier = std::stod(value);
        else if (key == "strategy.crypto_volume_change_amplification_factor") cfg.strategy.crypto_volume_change_amplification_factor = std::stod(value);
        else if (key == "strategy.percentage_calculation_multiplier") cfg.strategy.percentage_calculation_multiplier = std::stod(value);
        else if (key == "strategy.minimum_volume_threshold") cfg.strategy.minimum_volume_threshold = std::stod(value);
        else if (key == "strategy.rr_ratio") cfg.strategy.rr_ratio = std::stod(value);
        else if (key == "strategy.average_atr_comparison_multiplier") cfg.strategy.average_atr_comparison_multiplier = std::stoi(value);
        else if (key == "strategy.atr_absolute_minimum_threshold") cfg.strategy.atr_absolute_minimum_threshold = std::stod(value);
        else if (key == "strategy.use_absolute_atr_threshold_instead_of_relative") cfg.strategy.use_absolute_atr_threshold = (value == "true");
        
        // Momentum signal configuration
        else if (key == "strategy.minimum_price_change_percentage_for_momentum") cfg.strategy.minimum_price_change_percentage_for_momentum = std::stod(value);
        else if (key == "strategy.minimum_volume_increase_percentage_for_buy_signals") cfg.strategy.minimum_volume_increase_percentage_for_buy_signals = std::stod(value);
        else if (key == "strategy.minimum_volatility_percentage_for_buy_signals") cfg.strategy.minimum_volatility_percentage_for_buy_signals = std::stod(value);
        else if (key == "strategy.minimum_volume_increase_percentage_for_sell_signals") cfg.strategy.minimum_volume_increase_percentage_for_sell_signals = std::stod(value);
        else if (key == "strategy.minimum_volatility_percentage_for_sell_signals") cfg.strategy.minimum_volatility_percentage_for_sell_signals = std::stod(value);
        else if (key == "strategy.minimum_signal_strength_threshold") cfg.strategy.minimum_signal_strength_threshold = std::stod(value);
        
        // Signal strength weighting configuration
        else if (key == "strategy.basic_price_pattern_weight") cfg.strategy.basic_price_pattern_weight = std::stod(value);
        else if (key == "strategy.momentum_indicator_weight") cfg.strategy.momentum_indicator_weight = std::stod(value);
        else if (key == "strategy.volume_analysis_weight") cfg.strategy.volume_analysis_weight = std::stod(value);
        else if (key == "strategy.volatility_analysis_weight") cfg.strategy.volatility_analysis_weight = std::stod(value);
        
        // Doji pattern detection configuration
        else if (key == "strategy.doji_candlestick_body_size_threshold_percentage") cfg.strategy.doji_candlestick_body_size_threshold_percentage = std::stod(value);
        else if (key == "strategy.buy_signals_allow_equal_close") cfg.strategy.buy_signals_allow_equal_close = to_bool(value);
        else if (key == "strategy.buy_signals_require_higher_high") cfg.strategy.buy_signals_require_higher_high = to_bool(value);
        else if (key == "strategy.buy_signals_require_higher_low") cfg.strategy.buy_signals_require_higher_low = to_bool(value);
        else if (key == "strategy.sell_signals_allow_equal_close") cfg.strategy.sell_signals_allow_equal_close = to_bool(value);
        else if (key == "strategy.sell_signals_require_lower_low") cfg.strategy.sell_signals_require_lower_low = to_bool(value);
        else if (key == "strategy.sell_signals_require_lower_high") cfg.strategy.sell_signals_require_lower_high = to_bool(value);
        else if (key == "strategy.price_buffer_pct") cfg.strategy.price_buffer_pct = std::stod(value);
        else if (key == "strategy.min_price_buffer") cfg.strategy.min_price_buffer = std::stod(value);
        else if (key == "strategy.max_price_buffer") cfg.strategy.max_price_buffer = std::stod(value);
        else if (key == "strategy.stop_loss_buffer_amount_dollars") cfg.strategy.stop_loss_buffer_amount_dollars = std::stod(value);
        else if (key == "strategy.use_current_market_price_for_order_execution") cfg.strategy.use_current_market_price_for_order_execution = to_bool(value);
        else if (key == "strategy.profit_taking_threshold_dollars") cfg.strategy.profit_taking_threshold_dollars = std::stod(value);
        else if (key == "strategy.take_profit_percentage") cfg.strategy.take_profit_percentage = std::stod(value);
        else if (key == "strategy.use_take_profit_percentage") cfg.strategy.use_take_profit_percentage = to_bool(value);
        else if (key == "strategy.enable_fixed_share_quantity_per_trade") cfg.strategy.enable_fixed_share_quantity_per_trade = to_bool(value);
        else if (key == "strategy.enable_risk_based_position_multiplier") cfg.strategy.enable_risk_based_position_multiplier = to_bool(value);
        else if (key == "strategy.fixed_share_quantity_per_trade") cfg.strategy.fixed_share_quantity_per_trade = std::stoi(value);
        else if (key == "strategy.risk_based_position_size_multiplier") cfg.strategy.risk_based_position_size_multiplier = std::stod(value);
        else if (key == "strategy.maximum_share_quantity_per_single_trade") cfg.strategy.maximum_share_quantity_per_single_trade = std::stoi(value);
        else if (key == "strategy.minimum_acceptable_price_for_signals") cfg.strategy.minimum_acceptable_price_for_signals = std::stod(value);
        else if (key == "strategy.maximum_acceptable_price_for_signals") cfg.strategy.maximum_acceptable_price_for_signals = std::stod(value);
        
        // Strategy precision configuration
        else if (key == "strategy.ratio_display_precision") cfg.strategy.ratio_display_precision = std::stoi(value);
        else if (key == "strategy.factor_display_precision") cfg.strategy.factor_display_precision = std::stoi(value);
        else if (key == "strategy.atr_volume_display_precision") cfg.strategy.atr_volume_display_precision = std::stoi(value);

        // Short selling configuration
        else if (key == "strategy.enable_short_selling") cfg.strategy.enable_short_selling = to_bool(value);
        else if (key == "strategy.short_availability_check") cfg.strategy.short_availability_check = to_bool(value);
        else if (key == "strategy.default_shortable_quantity") cfg.strategy.default_shortable_quantity = std::stoi(value);
        else if (key == "strategy.existing_short_multiplier") cfg.strategy.existing_short_multiplier = std::stod(value);
        else if (key == "strategy.short_safety_margin") cfg.strategy.short_safety_margin = std::stod(value);
        else if (key == "strategy.short_retry_attempts") cfg.strategy.short_retry_attempts = std::stoi(value);
        else if (key == "strategy.short_retry_delay_ms") cfg.strategy.short_retry_delay_ms = std::stoi(value);

        // Risk Management
        else if (key == "risk.max_daily_loss_percentage") cfg.strategy.max_daily_loss_percentage = std::stod(value);
        else if (key == "risk.daily_profit_target_percentage") cfg.strategy.daily_profit_target_percentage = std::stod(value);
        else if (key == "risk.max_account_exposure_percentage") cfg.strategy.max_account_exposure_percentage = std::stod(value);
        else if (key == "risk.position_scaling_multiplier") cfg.strategy.position_scaling_multiplier = std::stod(value);
        else if (key == "risk.buying_power_utilization_percentage") cfg.strategy.buying_power_utilization_percentage = std::stod(value);
        else if (key == "risk.buying_power_validation_safety_margin") cfg.strategy.buying_power_validation_safety_margin = std::stod(value);
        else if (key == "risk.risk_percentage_per_trade") cfg.strategy.risk_percentage_per_trade = std::stod(value);
        else if (key == "risk.maximum_dollar_value_per_trade") cfg.strategy.maximum_dollar_value_per_trade = std::stod(value);
        else if (key == "risk.allow_multiple_positions_per_symbol") cfg.strategy.allow_multiple_positions_per_symbol = to_bool(value);
        else if (key == "risk.maximum_position_layers") cfg.strategy.maximum_position_layers = std::stoi(value);
        else if (key == "risk.close_positions_on_signal_reversal") cfg.strategy.close_positions_on_signal_reversal = to_bool(value);

        // Thread Polling Intervals
        else if (key == "timing.market_data_thread_polling_interval_seconds") cfg.timing.thread_market_data_poll_interval_sec = std::stoi(value);
        else if (key == "timing.account_data_thread_polling_interval_seconds") cfg.timing.thread_account_data_poll_interval_sec = std::stoi(value);
        else if (key == "timing.market_gate_thread_polling_interval_seconds") cfg.timing.thread_market_gate_poll_interval_sec = std::stoi(value);
        else if (key == "timing.trader_decision_thread_polling_interval_seconds") cfg.timing.thread_trader_poll_interval_sec = std::stoi(value);
        else if (key == "timing.logging_thread_polling_interval_seconds") cfg.timing.thread_logging_poll_interval_sec = std::stoi(value);

        // Market Session Buffer Times
        else if (key == "timing.pre_market_open_buffer_minutes") cfg.timing.pre_market_open_buffer_minutes = std::stoi(value);
        else if (key == "timing.post_market_close_buffer_minutes") cfg.timing.post_market_close_buffer_minutes = std::stoi(value);
        else if (key == "timing.market_close_grace_period_minutes") cfg.timing.market_close_grace_period_minutes = std::stoi(value);

        // Historical Data Configuration
        else if (key == "timing.historical_data_fetch_period_minutes") cfg.timing.historical_data_fetch_period_minutes = std::stoi(value);
        else if (key == "timing.historical_data_buffer_size") cfg.timing.historical_data_buffer_size = std::stoi(value);
        else if (key == "timing.account_data_cache_duration_seconds") cfg.timing.account_data_cache_duration_seconds = std::stoi(value);
        else if (key == "timing.market_data_staleness_threshold_seconds") cfg.timing.market_data_staleness_threshold_seconds = std::stoi(value);
        else if (key == "timing.crypto_data_staleness_threshold_seconds") cfg.timing.crypto_data_staleness_threshold_seconds = std::stoi(value);

        // System Health Monitoring
        else if (key == "timing.enable_system_health_monitoring") cfg.timing.enable_system_health_monitoring = to_bool(value);
        else if (key == "timing.system_health_logging_interval_seconds") cfg.timing.system_health_logging_interval_seconds = std::stoi(value);

        // Error Recovery Timing
        else if (key == "timing.emergency_trading_halt_duration_minutes") cfg.timing.emergency_trading_halt_duration_minutes = std::stoi(value);

        // User Interface Updates
        else if (key == "timing.countdown_display_refresh_interval_seconds") cfg.timing.countdown_display_refresh_interval_seconds = std::stoi(value);

        // Thread Lifecycle Management
        else if (key == "timing.thread_initialization_delay_milliseconds") cfg.timing.thread_initialization_delay_milliseconds = std::stoi(value);
        else if (key == "timing.thread_startup_sequence_delay_milliseconds") cfg.timing.thread_startup_sequence_delay_milliseconds = std::stoi(value);

        // Order Management Timing
        else if (key == "timing.order_cancellation_processing_delay_milliseconds") cfg.timing.order_cancellation_processing_delay_milliseconds = std::stoi(value);
        else if (key == "timing.position_verification_timeout_milliseconds") cfg.timing.position_verification_timeout_milliseconds = std::stoi(value);
        else if (key == "timing.position_settlement_timeout_milliseconds") cfg.timing.position_settlement_timeout_milliseconds = std::stoi(value);
        else if (key == "timing.maximum_concurrent_order_cancellations") cfg.timing.maximum_concurrent_order_cancellations = std::stoi(value);

        // Trading Safety Constraints
        else if (key == "timing.minimum_interval_between_orders_seconds") cfg.timing.minimum_interval_between_orders_seconds = std::stoi(value);
        else if (key == "timing.enable_wash_trade_prevention_mechanism") cfg.timing.enable_wash_trade_prevention_mechanism = to_bool(value);

        // Precision Settings for Metrics
        else if (key == "timing.cpu_usage_display_precision") cfg.timing.cpu_usage_display_precision = std::stoi(value);
        else if (key == "timing.performance_rate_display_precision") cfg.timing.performance_rate_display_precision = std::stoi(value);

        // Logging
        else if (key == "logging.log_file") cfg.logging.log_file = value;
        else if (key == "logging.max_log_file_size_mb") cfg.logging.max_log_file_size_mb = std::stoi(value);
        else if (key == "logging.log_backup_count") cfg.logging.log_backup_count = std::stoi(value);
        else if (key == "logging.console_log_level") cfg.logging.console_log_level = value;
        else if (key == "logging.file_log_level") cfg.logging.file_log_level = value;
        else if (key == "logging.include_timestamp") cfg.logging.include_timestamp = to_bool(value);
        else if (key == "logging.include_thread_id") cfg.logging.include_thread_id = to_bool(value);
        else if (key == "logging.include_function_name") cfg.logging.include_function_name = to_bool(value);
    }
    return true;
}

bool load_thread_configs(AlpacaTrader::Config::SystemConfig& cfg, const std::string& thread_config_path) {
    std::ifstream file(thread_config_path);
    if (!file.is_open()) {
        log_message("ERROR: Could not open thread config file: " + thread_config_path, "");
        return false;
    }

    // Helper function to convert priority string to enum
    auto parse_priority = [](const std::string& value) -> AlpacaTrader::Config::Priority {
        if (value == "REALTIME") return AlpacaTrader::Config::Priority::REALTIME;
        else if (value == "HIGHEST") return AlpacaTrader::Config::Priority::HIGHEST;
        else if (value == "HIGH") return AlpacaTrader::Config::Priority::HIGH;
        else if (value == "NORMAL") return AlpacaTrader::Config::Priority::NORMAL;
        else if (value == "LOW") return AlpacaTrader::Config::Priority::LOW;
        else if (value == "LOWEST") return AlpacaTrader::Config::Priority::LOWEST;
        else return AlpacaTrader::Config::Priority::NORMAL; // Default fallback
    };

    // Helper function to get thread settings for loading (creates entry if needed)
    auto get_thread_settings_for_loading = [&cfg](const std::string& thread_name) -> AlpacaTrader::Config::ThreadSettings* {
        return &cfg.thread_registry.get_thread_settings_for_loading(thread_name);
    };

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string key, value;
        if (!std::getline(ss, key, ',')) continue;
        if (!std::getline(ss, value)) continue;
        key = trim(key); value = trim(value);

        // Parse thread configuration with format: thread.{thread_name}.{property}
        if (key.substr(0, 7) == "thread.") {
            // Extract thread name and property
            size_t first_dot = key.find('.', 7);
            if (first_dot == std::string::npos) continue;

            std::string thread_name = key.substr(7, first_dot - 7);
            std::string property = key.substr(first_dot + 1);

            AlpacaTrader::Config::ThreadSettings* settings = get_thread_settings_for_loading(thread_name);

            // Set the appropriate property
            if (property == "priority") {
                settings->priority = parse_priority(value);
            }
            else if (property == "cpu_affinity") {
                settings->cpu_affinity = std::stoi(value);
            }
            else if (property == "name") {
                settings->name = value;
            }
            else if (property == "use_cpu_affinity") {
                settings->use_cpu_affinity = to_bool(value);
            }
            else {
                log_message("WARNING: Unknown thread property: " + property + " for thread: " + thread_name, "");
            }
        }
    }

    file.close();

    // Validate symbol consistency (single source of truth)
    if (cfg.trading_mode.primary_symbol.empty()) {
        log_message("ERROR: Primary trading symbol missing (provide via strategy_config.csv)", "");
        return false;
    }

    // Log successful loading of all discovered threads
    log_message("Thread configuration loaded successfully for " + std::to_string(cfg.thread_registry.thread_settings.size()) + " threads", "");
    return true;
}

int load_system_config(AlpacaTrader::Config::SystemConfig& config) {
    // Load configuration from separate logical files
    std::vector<std::string> config_files = {
        "config/api_endpoints_config.csv",
        "config/strategy_config.csv",
        "config/logging_config.csv",
        "config/thread_config.csv",
        "config/timing_config.csv"
    };

    // Load each configuration file
    for (const auto& config_path : config_files) {
        if (!load_config_from_csv(config, config_path)) {
            log_message("ERROR: Failed to load config CSV from " + config_path, "");
            return 1;
        }
    }

    // Load thread configurations
    std::string thread_config_path = std::string("config/thread_config.csv");
    if (!load_thread_configs(config, thread_config_path)) {
        log_message("ERROR: Failed to load thread configurations from " + thread_config_path, "");
        return 1;
    }

    // Validate configuration completeness
    std::string validation_error;
    if (!validate_config(config, validation_error)) {
        log_message("ERROR: Configuration validation failed: " + validation_error, "");
        return 1;
    }

    return 0;
}

bool validate_config(const AlpacaTrader::Config::SystemConfig& config, std::string& error_message) {
    // Validate multi-API configuration
    if (config.multi_api.providers.empty()) {
        error_message = "No API providers configured (provide via api_endpoints_config.csv)";
        return false;
    }

    // Validate strategy configuration (single source of truth)
    if (config.trading_mode.primary_symbol.empty()) {
        error_message = "Trading symbol missing (provide via strategy_config.csv)";
        return false;
    }
    if (config.strategy.minutes_per_bar <= 0) {
        error_message = "Invalid minutes_per_bar (must be > 0, provide via strategy_config.csv)";
        return false;
    }
    if (config.strategy.bars_to_fetch_for_calculations <= 0) {
        error_message = "Invalid bars_to_fetch_for_calculations (must be > 0, provide via strategy_config.csv)";
        return false;
    }

    // Validate trading mode configuration (single source of truth)
    if (config.trading_mode.primary_symbol.empty()) {
        error_message = "Trading mode configuration missing (provide via strategy_config.csv)";
        return false;
    }
    
    // Validate crypto asset configuration (single source of truth)
    if (config.trading_mode.primary_symbol.find('/') != std::string::npos && config.trading_mode.mode != AlpacaTrader::Config::TradingMode::CRYPTO) {
        error_message = "Crypto symbol format detected (" + config.trading_mode.primary_symbol + ") but trading_mode.mode is not crypto - set trading_mode.mode=crypto in strategy_config.csv";
        return false;
    }
    if (config.strategy.atr_calculation_period < 2) {
        error_message = "strategy.atr_calculation_period must be >= 2 (ATR calculation period)";
        return false;
    }

    // Validate new volatility calculation parameters
    if (config.strategy.bars_to_fetch_for_calculations < 1) {
        error_message = "strategy.bars_to_fetch_for_calculations must be >= 1";
        return false;
    }

    if (config.strategy.minutes_per_bar < 1) {
        error_message = "strategy.minutes_per_bar must be >= 1";
        return false;
    }

    if (config.strategy.atr_calculation_bars < 2) {
        error_message = "strategy.atr_calculation_bars must be >= 2";
        return false;
    }

    if (config.strategy.daily_bars_timeframe.empty()) {
        error_message = "strategy.daily_bars_timeframe cannot be empty";
        return false;
    }

    if (config.strategy.daily_bars_count < 1) {
        error_message = "strategy.daily_bars_count must be >= 1";
        return false;
    }
    if (config.strategy.rr_ratio <= 0.0) {
        error_message = "strategy.rr_ratio must be > 0 (risk/reward ratio)";
        return false;
    }

    // Validate risk percentage is reasonable
    if (config.strategy.risk_percentage_per_trade <= 0.0 || config.strategy.risk_percentage_per_trade > 10.0) {
        error_message = "strategy.risk_percentage_per_trade must be between 0.0 and 10.0 (0% to 1000%)";
        return false;
    }

    // Validate account exposure percentage
    if (config.strategy.max_account_exposure_percentage <= 0.0 || config.strategy.max_account_exposure_percentage > 100.0) {
        error_message = "strategy.max_account_exposure_percentage must be between 0.0 and 100.0 (0% to 100%)";
        return false;
    }

    // Validate take profit configuration
    if (config.strategy.take_profit_percentage < 0.0 || config.strategy.take_profit_percentage > 1.0) {
        error_message = "strategy.take_profit_percentage must be between 0.0 and 1.0 (0% to 100%)";
        return false;
    }

    // Validate ATR calculation period
    if (config.strategy.atr_calculation_period < 2 || config.strategy.atr_calculation_period > 100) {
        error_message = "strategy.atr_calculation_period must be between 2 and 100";
        return false;
    }

    // Validate new ATR calculation bars
    if (config.strategy.atr_calculation_bars < 2 || config.strategy.atr_calculation_bars > 100) {
        error_message = "strategy.atr_calculation_bars must be between 2 and 100";
        return false;
    }

    // Validate daily bars timeframe
    if (config.strategy.daily_bars_timeframe.empty()) {
        error_message = "strategy.daily_bars_timeframe cannot be empty";
        return false;
    }

    // Validate daily bars count
    if (config.strategy.daily_bars_count < 1) {
        error_message = "strategy.daily_bars_count must be >= 1";
        return false;
    }

    // Validate minimum signal strength threshold
    if (config.strategy.minimum_signal_strength_threshold < 0.0 || config.strategy.minimum_signal_strength_threshold > 1.0) {
        error_message = "strategy.minimum_signal_strength_threshold must be between 0.0 and 1.0";
        return false;
    }

    // Validate thread polling intervals are reasonable
    if (config.timing.thread_market_data_poll_interval_sec <= 0 || config.timing.thread_market_data_poll_interval_sec > 3600) {
        error_message = "timing.thread_market_data_poll_interval_sec must be between 1 and 3600 seconds";
        return false;
    }
    
    // Protection: Ensure only one take profit method is enabled
    if (config.strategy.use_take_profit_percentage && config.strategy.rr_ratio > 0.0) {
        // If percentage is enabled, disable risk/reward ratio for take profit
        // (risk/reward can still be used for stop loss calculation)
    }
    if (config.strategy.enable_fixed_share_quantity_per_trade && config.strategy.enable_risk_based_position_multiplier) {
        error_message = "Only one position sizing method can be enabled at a time";
        return false;
    }
    if (config.timing.thread_market_data_poll_interval_sec <= 0 || config.timing.thread_account_data_poll_interval_sec <= 0) {
        error_message = "timing polling intervals must be > 0 (thread polling interval seconds)";
        return false;
    }
    return true;
}

