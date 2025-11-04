#include "config_loader.hpp"
#include "multi_api_config_loader.hpp"
#include "configs/system_config.hpp"
#include "configs/thread_config.hpp"
#include "logging/logger/logging_macros.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

using AlpacaTrader::Logging::log_message;

namespace {
    inline std::string trim(const std::string& input_string) {
        const char* whitespace_chars = " \t\r\n";
        auto begin_position = input_string.find_first_not_of(whitespace_chars);
        auto end_position = input_string.find_last_not_of(whitespace_chars);
        if (begin_position == std::string::npos) return "";
        return input_string.substr(begin_position, end_position - begin_position + 1);
    }

    inline bool to_bool(const std::string& input_value) {
        std::string normalized_value = input_value; 
        std::transform(normalized_value.begin(), normalized_value.end(), normalized_value.begin(), ::tolower);
        return normalized_value == "1" || normalized_value == "true" || normalized_value == "yes";
    }

}

bool load_config_from_csv(AlpacaTrader::Config::SystemConfig& cfg, const std::string& csv_path) {
    try {
    // Load multi-API configuration only from api_endpoints_config.csv
    if (csv_path.find("api_endpoints_config.csv") != std::string::npos) {
        try {
            cfg.multi_api = AlpacaTrader::Core::MultiApiConfigLoader::load_from_csv(csv_path);
        } catch (const std::exception& config_exception_error) {
            log_message("Failed to load multi-API configuration: " + std::string(config_exception_error.what()), "");
            return false;
        }
    }
    
    std::ifstream config_file_stream(csv_path);
        if (!config_file_stream.is_open()) {
            return false;
        }
    std::string config_line_string;
    while (std::getline(config_file_stream, config_line_string)) {
            try {
        if (config_line_string.empty()) continue;
                config_line_string = trim(config_line_string);
                if (config_line_string.empty() || config_line_string[0] == '#') continue;
        std::stringstream config_line_stream(config_line_string);
        std::string config_key_string, config_value_string;
        if (!std::getline(config_line_stream, config_key_string, ',')) continue;
        if (!std::getline(config_line_stream, config_value_string)) continue;
                config_key_string = trim(config_key_string); 
                config_value_string = trim(config_value_string);

        // Trading Mode Configuration (only from strategy_config.csv)
        if (csv_path.find("strategy_config.csv") != std::string::npos) {
            if (config_key_string == "trading_mode.mode") {
                if (config_value_string.empty()) {
                    throw std::runtime_error("Trading mode is required but not provided");
                }
                cfg.trading_mode.mode = AlpacaTrader::Config::TradingModeConfig::parse_mode(config_value_string);
                // Map trading mode to strategy crypto asset indicator
                cfg.strategy.is_crypto_asset = (cfg.trading_mode.mode == AlpacaTrader::Config::TradingMode::CRYPTO);
            }
            else if (config_key_string == "trading_mode.primary_symbol") {
                if (config_value_string.empty()) {
                    throw std::runtime_error("Primary symbol is required but not provided");
                }
                cfg.trading_mode.primary_symbol = config_value_string;
                // Map primary symbol to strategy symbol
                cfg.strategy.symbol = config_value_string;
            }
        }

        // All API configuration handled by multi_api section

        // Strategy Configuration - session and other settings
        if (config_key_string == "session.et_utc_offset_hours") cfg.strategy.et_utc_offset_hours = std::stoi(config_value_string);
        else if (config_key_string == "session.market_open_hour") cfg.strategy.market_open_hour = std::stoi(config_value_string);
        else if (config_key_string == "session.market_open_minute") cfg.strategy.market_open_minute = std::stoi(config_value_string);
        else if (config_key_string == "session.market_close_hour") cfg.strategy.market_close_hour = std::stoi(config_value_string);
        else if (config_key_string == "session.market_close_minute") cfg.strategy.market_close_minute = std::stoi(config_value_string);

        // Strategy parameters
        else if (config_key_string == "strategy.bars_to_fetch_for_calculations") cfg.strategy.bars_to_fetch_for_calculations = std::stoi(config_value_string);
        else if (config_key_string == "strategy.minutes_per_bar") cfg.strategy.minutes_per_bar = std::stoi(config_value_string);
        else if (config_key_string == "strategy.atr_calculation_bars") cfg.strategy.atr_calculation_bars = std::stoi(config_value_string);
        else if (config_key_string == "strategy.minimum_bars_for_atr_calculation") cfg.strategy.minimum_bars_for_atr_calculation = std::stoi(config_value_string);
        else if (config_key_string == "strategy.daily_bars_timeframe") cfg.strategy.daily_bars_timeframe = config_value_string;
        else if (config_key_string == "strategy.daily_bars_count") cfg.strategy.daily_bars_count = std::stoi(config_value_string);
        else if (config_key_string == "strategy.minimum_data_accumulation_seconds_before_trading") cfg.strategy.minimum_data_accumulation_seconds_before_trading = std::stoi(config_value_string);
        else if (config_key_string == "strategy.entry_signal_atr_multiplier") cfg.strategy.entry_signal_atr_multiplier = std::stod(config_value_string);
        else if (config_key_string == "strategy.entry_signal_volume_multiplier") cfg.strategy.entry_signal_volume_multiplier = std::stod(config_value_string);
        else if (config_key_string == "strategy.crypto_volume_multiplier") cfg.strategy.crypto_volume_multiplier = std::stod(config_value_string);
        else if (config_key_string == "strategy.crypto_volume_change_amplification_factor") cfg.strategy.crypto_volume_change_amplification_factor = std::stod(config_value_string);
        else if (config_key_string == "strategy.percentage_calculation_multiplier") cfg.strategy.percentage_calculation_multiplier = std::stod(config_value_string);
        else if (config_key_string == "strategy.minimum_volume_threshold") cfg.strategy.minimum_volume_threshold = std::stod(config_value_string);
        else if (config_key_string == "strategy.rr_ratio") cfg.strategy.rr_ratio = std::stod(config_value_string);
        else if (config_key_string == "strategy.average_atr_comparison_multiplier") cfg.strategy.average_atr_comparison_multiplier = std::stoi(config_value_string);
        else if (config_key_string == "strategy.atr_absolute_minimum_threshold") cfg.strategy.atr_absolute_minimum_threshold = std::stod(config_value_string);
        else if (config_key_string == "strategy.use_absolute_atr_threshold_instead_of_relative") cfg.strategy.use_absolute_atr_threshold = (config_value_string == "true");
        
        // Momentum signal configuration
                else if (config_key_string == "strategy.minimum_price_change_percentage_for_momentum") {
                    cfg.strategy.minimum_price_change_percentage_for_momentum = std::stod(config_value_string);
                }
        else if (config_key_string == "strategy.minimum_volume_increase_percentage_for_buy_signals") cfg.strategy.minimum_volume_increase_percentage_for_buy_signals = std::stod(config_value_string);
        else if (config_key_string == "strategy.minimum_volatility_percentage_for_buy_signals") cfg.strategy.minimum_volatility_percentage_for_buy_signals = std::stod(config_value_string);
        else if (config_key_string == "strategy.minimum_volume_increase_percentage_for_sell_signals") cfg.strategy.minimum_volume_increase_percentage_for_sell_signals = std::stod(config_value_string);
        else if (config_key_string == "strategy.minimum_volatility_percentage_for_sell_signals") cfg.strategy.minimum_volatility_percentage_for_sell_signals = std::stod(config_value_string);
        else if (config_key_string == "strategy.minimum_signal_strength_threshold") cfg.strategy.minimum_signal_strength_threshold = std::stod(config_value_string);
        
        // Signal strength weighting configuration
        else if (config_key_string == "strategy.basic_price_pattern_weight") cfg.strategy.basic_price_pattern_weight = std::stod(config_value_string);
        else if (config_key_string == "strategy.momentum_indicator_weight") cfg.strategy.momentum_indicator_weight = std::stod(config_value_string);
        else if (config_key_string == "strategy.volume_analysis_weight") cfg.strategy.volume_analysis_weight = std::stod(config_value_string);
        else if (config_key_string == "strategy.volatility_analysis_weight") cfg.strategy.volatility_analysis_weight = std::stod(config_value_string);
        
        // Doji pattern detection configuration
        else if (config_key_string == "strategy.doji_candlestick_body_size_threshold_percentage") cfg.strategy.doji_candlestick_body_size_threshold_percentage = std::stod(config_value_string);
        else if (config_key_string == "strategy.buy_signals_allow_equal_close") cfg.strategy.buy_signals_allow_equal_close = to_bool(config_value_string);
        else if (config_key_string == "strategy.buy_signals_require_higher_high") cfg.strategy.buy_signals_require_higher_high = to_bool(config_value_string);
        else if (config_key_string == "strategy.buy_signals_require_higher_low") cfg.strategy.buy_signals_require_higher_low = to_bool(config_value_string);
        else if (config_key_string == "strategy.sell_signals_allow_equal_close") cfg.strategy.sell_signals_allow_equal_close = to_bool(config_value_string);
        else if (config_key_string == "strategy.sell_signals_require_lower_low") cfg.strategy.sell_signals_require_lower_low = to_bool(config_value_string);
        else if (config_key_string == "strategy.sell_signals_require_lower_high") cfg.strategy.sell_signals_require_lower_high = to_bool(config_value_string);
        else if (config_key_string == "strategy.price_buffer_pct") cfg.strategy.price_buffer_pct = std::stod(config_value_string);
        else if (config_key_string == "strategy.min_price_buffer") cfg.strategy.min_price_buffer = std::stod(config_value_string);
        else if (config_key_string == "strategy.max_price_buffer") cfg.strategy.max_price_buffer = std::stod(config_value_string);
        else if (config_key_string == "strategy.stop_loss_buffer_amount_dollars") cfg.strategy.stop_loss_buffer_amount_dollars = std::stod(config_value_string);
        else if (config_key_string == "strategy.use_current_market_price_for_order_execution") cfg.strategy.use_current_market_price_for_order_execution = to_bool(config_value_string);
        else if (config_key_string == "strategy.profit_taking_threshold_dollars") cfg.strategy.profit_taking_threshold_dollars = std::stod(config_value_string);
        
        // System monitoring configuration (support both monitoring.* and strategy.* prefixes)
        else if (config_key_string == "strategy.max_failure_rate_pct" || config_key_string == "monitoring.max_failure_rate_pct") cfg.strategy.max_failure_rate_pct = std::stod(config_value_string);
        else if (config_key_string == "strategy.max_drawdown_pct" || config_key_string == "monitoring.max_drawdown_pct") cfg.strategy.max_drawdown_pct = std::stod(config_value_string);
        else if (config_key_string == "strategy.max_data_age_min" || config_key_string == "monitoring.max_data_age_min") cfg.strategy.max_data_age_min = std::stoi(config_value_string);
        else if (config_key_string == "strategy.max_inactivity_min" || config_key_string == "monitoring.max_inactivity_min") cfg.strategy.max_inactivity_min = std::stoi(config_value_string);
        else if (config_key_string == "strategy.health_check_interval_sec" || config_key_string == "monitoring.health_check_interval_sec") cfg.strategy.health_check_interval_sec = std::stoi(config_value_string);
        else if (config_key_string == "strategy.performance_report_interval_min" || config_key_string == "monitoring.performance_report_interval_min") cfg.strategy.performance_report_interval_min = std::stoi(config_value_string);
        else if (config_key_string == "strategy.alert_on_failure_rate" || config_key_string == "monitoring.alert_on_failure_rate") cfg.strategy.alert_on_failure_rate = to_bool(config_value_string);
        else if (config_key_string == "strategy.alert_on_drawdown" || config_key_string == "monitoring.alert_on_drawdown") cfg.strategy.alert_on_drawdown = to_bool(config_value_string);
        else if (config_key_string == "strategy.alert_on_data_stale" || config_key_string == "monitoring.alert_on_data_stale") cfg.strategy.alert_on_data_stale = to_bool(config_value_string);
        else if (config_key_string == "strategy.take_profit_percentage") cfg.strategy.take_profit_percentage = std::stod(config_value_string);
        else if (config_key_string == "strategy.use_take_profit_percentage") cfg.strategy.use_take_profit_percentage = to_bool(config_value_string);
        else if (config_key_string == "strategy.enable_fixed_share_quantity_per_trade") cfg.strategy.enable_fixed_share_quantity_per_trade = to_bool(config_value_string);
        else if (config_key_string == "strategy.enable_risk_based_position_multiplier") cfg.strategy.enable_risk_based_position_multiplier = to_bool(config_value_string);
        else if (config_key_string == "strategy.fixed_share_quantity_per_trade") cfg.strategy.fixed_share_quantity_per_trade = std::stoi(config_value_string);
        else if (config_key_string == "strategy.risk_based_position_size_multiplier") cfg.strategy.risk_based_position_size_multiplier = std::stod(config_value_string);
        else if (config_key_string == "strategy.maximum_share_quantity_per_single_trade") cfg.strategy.maximum_share_quantity_per_single_trade = std::stoi(config_value_string);
        else if (config_key_string == "strategy.maximum_dollar_value_per_single_trade") {
            try {
                double parsed_value = std::stod(config_value_string);
                if (parsed_value <= 0.0) {
                    throw std::runtime_error("strategy.maximum_dollar_value_per_single_trade must be > 0.0, got: " + config_value_string);
                }
                cfg.strategy.maximum_dollar_value_per_single_trade = parsed_value;
                log_message("Loaded strategy.maximum_dollar_value_per_single_trade = " + std::to_string(cfg.strategy.maximum_dollar_value_per_single_trade), "");
            } catch (const std::exception& parse_exception_error) {
                throw std::runtime_error("Failed to parse strategy.maximum_dollar_value_per_single_trade from value '" + config_value_string + "': " + std::string(parse_exception_error.what()));
            }
        }
        else if (config_key_string == "strategy.minimum_acceptable_price_for_signals") cfg.strategy.minimum_acceptable_price_for_signals = std::stod(config_value_string);
        else if (config_key_string == "strategy.maximum_acceptable_price_for_signals") cfg.strategy.maximum_acceptable_price_for_signals = std::stod(config_value_string);
        
        // Strategy precision configuration
        else if (config_key_string == "strategy.ratio_display_precision") cfg.strategy.ratio_display_precision = std::stoi(config_value_string);
        else if (config_key_string == "strategy.factor_display_precision") cfg.strategy.factor_display_precision = std::stoi(config_value_string);
        else if (config_key_string == "strategy.atr_volume_display_precision") cfg.strategy.atr_volume_display_precision = std::stoi(config_value_string);
        
        // Signal and position label configuration
        else if (config_key_string == "strategy.signal_buy_string") {
            if (config_value_string.empty()) {
                throw std::runtime_error("Signal buy string is required but not provided");
            }
            cfg.strategy.signal_buy_string = config_value_string;
        }
        else if (config_key_string == "strategy.signal_sell_string") {
            if (config_value_string.empty()) {
                throw std::runtime_error("Signal sell string is required but not provided");
            }
            cfg.strategy.signal_sell_string = config_value_string;
        }
        else if (config_key_string == "strategy.position_long_string") {
            if (config_value_string.empty()) {
                throw std::runtime_error("Position long string is required but not provided");
            }
            cfg.strategy.position_long_string = config_value_string;
        }
        else if (config_key_string == "strategy.position_short_string") {
            if (config_value_string.empty()) {
                throw std::runtime_error("Position short string is required but not provided");
            }
            cfg.strategy.position_short_string = config_value_string;
        }

        // Short selling configuration
        else if (config_key_string == "strategy.enable_short_selling") cfg.strategy.enable_short_selling = to_bool(config_value_string);
        else if (config_key_string == "strategy.short_availability_check") cfg.strategy.short_availability_check = to_bool(config_value_string);
        else if (config_key_string == "strategy.default_shortable_quantity") cfg.strategy.default_shortable_quantity = std::stoi(config_value_string);
        else if (config_key_string == "strategy.existing_short_multiplier") cfg.strategy.existing_short_multiplier = std::stod(config_value_string);
        else if (config_key_string == "strategy.short_safety_margin") cfg.strategy.short_safety_margin = std::stod(config_value_string);
        else if (config_key_string == "strategy.short_retry_attempts") cfg.strategy.short_retry_attempts = std::stoi(config_value_string);
        else if (config_key_string == "strategy.short_retry_delay_ms") cfg.strategy.short_retry_delay_ms = std::stoi(config_value_string);

        // Risk Management
        else if (config_key_string == "risk.max_daily_loss_percentage") cfg.strategy.max_daily_loss_percentage = std::stod(config_value_string);
        else if (config_key_string == "risk.daily_profit_target_percentage") cfg.strategy.daily_profit_target_percentage = std::stod(config_value_string);
        else if (config_key_string == "risk.max_account_exposure_percentage") cfg.strategy.max_account_exposure_percentage = std::stod(config_value_string);
        else if (config_key_string == "risk.position_scaling_multiplier") cfg.strategy.position_scaling_multiplier = std::stod(config_value_string);
        else if (config_key_string == "risk.buying_power_utilization_percentage") cfg.strategy.buying_power_utilization_percentage = std::stod(config_value_string);
        else if (config_key_string == "risk.buying_power_validation_safety_margin") cfg.strategy.buying_power_validation_safety_margin = std::stod(config_value_string);
        else if (config_key_string == "risk.risk_percentage_per_trade") cfg.strategy.risk_percentage_per_trade = std::stod(config_value_string);
        else if (config_key_string == "risk.maximum_dollar_value_per_trade") {
            try {
                double parsed_value = std::stod(config_value_string);
                if (parsed_value <= 0.0) {
                    throw std::runtime_error("risk.maximum_dollar_value_per_trade must be > 0.0, got: " + config_value_string);
                }
                cfg.strategy.maximum_dollar_value_per_trade = parsed_value;
                log_message("Loaded risk.maximum_dollar_value_per_trade = " + std::to_string(cfg.strategy.maximum_dollar_value_per_trade), "");
            } catch (const std::exception& parse_exception_error) {
                throw std::runtime_error("Failed to parse risk.maximum_dollar_value_per_trade from value '" + config_value_string + "': " + std::string(parse_exception_error.what()));
            }
        }
        else if (config_key_string == "risk.allow_multiple_positions_per_symbol") cfg.strategy.allow_multiple_positions_per_symbol = to_bool(config_value_string);
        else if (config_key_string == "risk.maximum_position_layers") cfg.strategy.maximum_position_layers = std::stoi(config_value_string);
        else if (config_key_string == "risk.close_positions_on_signal_reversal") cfg.strategy.close_positions_on_signal_reversal = to_bool(config_value_string);

        // Thread Polling Intervals
        else if (config_key_string == "timing.market_data_thread_polling_interval_seconds") cfg.timing.thread_market_data_poll_interval_sec = std::stoi(config_value_string);
        else if (config_key_string == "timing.account_data_thread_polling_interval_seconds") cfg.timing.thread_account_data_poll_interval_sec = std::stoi(config_value_string);
        else if (config_key_string == "timing.market_gate_thread_polling_interval_seconds") cfg.timing.thread_market_gate_poll_interval_sec = std::stoi(config_value_string);
        else if (config_key_string == "timing.trader_decision_thread_polling_interval_seconds") cfg.timing.thread_trader_poll_interval_sec = std::stoi(config_value_string);
        else if (config_key_string == "timing.logging_thread_polling_interval_seconds") cfg.timing.thread_logging_poll_interval_sec = std::stoi(config_value_string);

        // Market Session Buffer Times
        else if (config_key_string == "timing.pre_market_open_buffer_minutes") cfg.timing.pre_market_open_buffer_minutes = std::stoi(config_value_string);
        else if (config_key_string == "timing.post_market_close_buffer_minutes") cfg.timing.post_market_close_buffer_minutes = std::stoi(config_value_string);
        else if (config_key_string == "timing.market_close_grace_period_minutes") cfg.timing.market_close_grace_period_minutes = std::stoi(config_value_string);

        // Historical Data Configuration
        else if (config_key_string == "timing.historical_data_fetch_period_minutes") cfg.timing.historical_data_fetch_period_minutes = std::stoi(config_value_string);
        else if (config_key_string == "timing.historical_data_buffer_size") cfg.timing.historical_data_buffer_size = std::stoi(config_value_string);
        else if (config_key_string == "timing.account_data_cache_duration_seconds") cfg.timing.account_data_cache_duration_seconds = std::stoi(config_value_string);
        else if (config_key_string == "timing.market_data_staleness_threshold_seconds") cfg.timing.market_data_staleness_threshold_seconds = std::stoi(config_value_string);
        else if (config_key_string == "timing.crypto_data_staleness_threshold_seconds") cfg.timing.crypto_data_staleness_threshold_seconds = std::stoi(config_value_string);
        else if (config_key_string == "timing.data_availability_wait_timeout_seconds") cfg.timing.data_availability_wait_timeout_seconds = std::stoi(config_value_string);

        // System Health Monitoring
        else if (config_key_string == "timing.enable_system_health_monitoring") cfg.timing.enable_system_health_monitoring = to_bool(config_value_string);
        else if (config_key_string == "timing.system_health_logging_interval_seconds") cfg.timing.system_health_logging_interval_seconds = std::stoi(config_value_string);
        else if (config_key_string == "timing.max_health_check_interval_minutes") cfg.timing.max_health_check_interval_minutes = std::stoi(config_value_string);

        // Error Recovery Timing
        else if (config_key_string == "timing.emergency_trading_halt_duration_minutes") cfg.timing.emergency_trading_halt_duration_minutes = std::stoi(config_value_string);

        // User Interface Updates
        else if (config_key_string == "timing.countdown_display_refresh_interval_seconds") cfg.timing.countdown_display_refresh_interval_seconds = std::stoi(config_value_string);

        // Thread Lifecycle Management
        else if (config_key_string == "timing.thread_initialization_delay_milliseconds") cfg.timing.thread_initialization_delay_milliseconds = std::stoi(config_value_string);
        else if (config_key_string == "timing.thread_startup_sequence_delay_milliseconds") cfg.timing.thread_startup_sequence_delay_milliseconds = std::stoi(config_value_string);

        // Order Management Timing
        else if (config_key_string == "timing.order_cancellation_processing_delay_milliseconds") cfg.timing.order_cancellation_processing_delay_milliseconds = std::stoi(config_value_string);
        else if (config_key_string == "timing.position_verification_timeout_milliseconds") cfg.timing.position_verification_timeout_milliseconds = std::stoi(config_value_string);
        else if (config_key_string == "timing.position_settlement_timeout_milliseconds") cfg.timing.position_settlement_timeout_milliseconds = std::stoi(config_value_string);
        else if (config_key_string == "timing.maximum_concurrent_order_cancellations") cfg.timing.maximum_concurrent_order_cancellations = std::stoi(config_value_string);
        else if (config_key_string == "timing.maximum_position_verification_attempts") cfg.timing.maximum_position_verification_attempts = std::stoi(config_value_string);

        // Trading Safety Constraints
        else if (config_key_string == "timing.minimum_interval_between_orders_seconds") cfg.timing.minimum_interval_between_orders_seconds = std::stoi(config_value_string);
        else if (config_key_string == "timing.enable_wash_trade_prevention_mechanism") cfg.timing.enable_wash_trade_prevention_mechanism = to_bool(config_value_string);

        // Precision Settings for Metrics
        else if (config_key_string == "timing.cpu_usage_display_precision") cfg.timing.cpu_usage_display_precision = std::stoi(config_value_string);
        else if (config_key_string == "timing.performance_rate_display_precision") cfg.timing.performance_rate_display_precision = std::stoi(config_value_string);

        // Connectivity Retry Configuration
        else if (config_key_string == "timing.connectivity_max_retry_delay_seconds") cfg.timing.connectivity_max_retry_delay_seconds = std::stoi(config_value_string);
        else if (config_key_string == "timing.connectivity_degraded_threshold") cfg.timing.connectivity_degraded_threshold = std::stoi(config_value_string);
        else if (config_key_string == "timing.connectivity_disconnected_threshold") cfg.timing.connectivity_disconnected_threshold = std::stoi(config_value_string);
        else if (config_key_string == "timing.connectivity_backoff_multiplier") cfg.timing.connectivity_backoff_multiplier = std::stod(config_value_string);

        // Logging
        else if (config_key_string == "logging.log_file") cfg.logging.log_file = config_value_string;
        else if (config_key_string == "logging.max_log_file_size_mb") cfg.logging.max_log_file_size_mb = std::stoi(config_value_string);
        else if (config_key_string == "logging.log_backup_count") cfg.logging.log_backup_count = std::stoi(config_value_string);
        else if (config_key_string == "logging.console_log_level") cfg.logging.console_log_level = config_value_string;
        else if (config_key_string == "logging.file_log_level") cfg.logging.file_log_level = config_value_string;
        else if (config_key_string == "logging.include_timestamp") cfg.logging.include_timestamp = to_bool(config_value_string);
        else if (config_key_string == "logging.include_thread_id") cfg.logging.include_thread_id = to_bool(config_value_string);
        else if (config_key_string == "logging.include_function_name") cfg.logging.include_function_name = to_bool(config_value_string);
            } catch (const std::exception& line_exception_error) {
                // COMPLIANCE: Fail hard on config parsing errors - no silent failures
                // Log the error and re-throw to fail hard
                log_message("CRITICAL: Error parsing config line: " + config_line_string + " - " + std::string(line_exception_error.what()), "");
                throw; // Re-throw to fail hard
            } catch (...) {
                // COMPLIANCE: Fail hard on unknown config parsing errors
                log_message("CRITICAL: Unknown error parsing config line: " + config_line_string, "");
                throw std::runtime_error("Unknown error parsing config line: " + config_line_string);
            }
        }
    return true;
    } catch (const std::exception& exception_error) {
        log_message("Exception in load_config_from_csv: " + std::string(exception_error.what()), "");
        return false;
    } catch (...) {
        log_message("Unknown exception in load_config_from_csv", "");
        return false;
    }
}

bool load_thread_configs(AlpacaTrader::Config::SystemConfig& cfg, const std::string& thread_config_path) {
    std::ifstream file(thread_config_path);
    if (!file.is_open()) {
        log_message("ERROR: Could not open thread config file: " + thread_config_path, "");
        return false;
    }

    // Helper function to convert priority string to enum
    auto parse_priority = [](const std::string& priority_value_string) -> AlpacaTrader::Config::Priority {
        if (priority_value_string == "REALTIME") return AlpacaTrader::Config::Priority::REALTIME;
        else if (priority_value_string == "HIGHEST") return AlpacaTrader::Config::Priority::HIGHEST;
        else if (priority_value_string == "HIGH") return AlpacaTrader::Config::Priority::HIGH;
        else if (priority_value_string == "NORMAL") return AlpacaTrader::Config::Priority::NORMAL;
        else if (priority_value_string == "LOW") return AlpacaTrader::Config::Priority::LOW;
        else if (priority_value_string == "LOWEST") return AlpacaTrader::Config::Priority::LOWEST;
        else throw std::runtime_error("Invalid thread priority: '" + priority_value_string + "' (must be REALTIME, HIGHEST, HIGH, NORMAL, LOW, or LOWEST - no defaults allowed)");
    };

    // Helper function to get thread settings for loading (creates entry if needed)
    auto get_thread_settings_for_loading = [&cfg](const std::string& thread_name) -> AlpacaTrader::Config::ThreadSettings* {
        return &cfg.thread_registry.get_thread_settings_for_loading(thread_name);
    };

    std::string thread_config_line_string;
    while (std::getline(file, thread_config_line_string)) {
        // Skip empty lines and comments
        if (thread_config_line_string.empty() || thread_config_line_string[0] == '#') continue;

        std::istringstream thread_config_line_stream(thread_config_line_string);
        std::string thread_config_key_string, thread_config_value_string;
        if (!std::getline(thread_config_line_stream, thread_config_key_string, ',')) continue;
        if (!std::getline(thread_config_line_stream, thread_config_value_string)) continue;
        thread_config_key_string = trim(thread_config_key_string); thread_config_value_string = trim(thread_config_value_string);

        // Parse thread configuration with format: thread.{thread_name}.{property}
        if (thread_config_key_string.substr(0, 7) == "thread.") {
            // Extract thread name and property
            size_t first_dot_position = thread_config_key_string.find('.', 7);
            if (first_dot_position == std::string::npos) continue;

            std::string thread_name_string = thread_config_key_string.substr(7, first_dot_position - 7);
            std::string thread_property_string = thread_config_key_string.substr(first_dot_position + 1);

            AlpacaTrader::Config::ThreadSettings* thread_settings_pointer = get_thread_settings_for_loading(thread_name_string);

            // Set the appropriate property
            if (thread_property_string == "priority") {
                thread_settings_pointer->priority = parse_priority(thread_config_value_string);
            }
            else if (thread_property_string == "cpu_affinity") {
                thread_settings_pointer->cpu_affinity = std::stoi(thread_config_value_string);
            }
            else if (thread_property_string == "name") {
                thread_settings_pointer->name = thread_config_value_string;
            }
            else if (thread_property_string == "use_cpu_affinity") {
                thread_settings_pointer->use_cpu_affinity = to_bool(thread_config_value_string);
            }
            else {
                log_message("WARNING: Unknown thread property: " + thread_property_string + " for thread: " + thread_name_string, "");
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
    // Validate volatility calculation parameters
    if (config.strategy.bars_to_fetch_for_calculations < 1) {
        error_message = "strategy.bars_to_fetch_for_calculations must be >= 1";
        return false;
    }

    if (config.strategy.minutes_per_bar < 1) {
        error_message = "strategy.minutes_per_bar must be >= 1";
        return false;
    }

    if (config.strategy.atr_calculation_bars < 1) {
        error_message = "strategy.atr_calculation_bars must be >= 1";
        return false;
    }
    
    // Validate bars_to_fetch_for_calculations is sufficient for all indicator calculations
    // Maximum needed: avg_atr uses (atr_calculation_bars * average_atr_comparison_multiplier + 1) bars
    int maximum_bars_needed_for_calculations_value = (config.strategy.atr_calculation_bars * config.strategy.average_atr_comparison_multiplier) + 1;
    if (config.strategy.bars_to_fetch_for_calculations < maximum_bars_needed_for_calculations_value) {
        error_message = "strategy.bars_to_fetch_for_calculations (" + std::to_string(config.strategy.bars_to_fetch_for_calculations) + 
                        ") must be >= " + std::to_string(maximum_bars_needed_for_calculations_value) + 
                        " (required for avg_atr calculation: atr_calculation_bars * average_atr_comparison_multiplier + 1)";
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

    // Validate ATR calculation bars
    if (config.strategy.atr_calculation_bars < 1 || config.strategy.atr_calculation_bars > 100) {
        error_message = "strategy.atr_calculation_bars must be between 1 and 100";
        return false;
    }
    
    if (config.strategy.minimum_bars_for_atr_calculation < 1) {
        error_message = "strategy.minimum_bars_for_atr_calculation must be >= 1";
        return false;
    }
    
    if (config.strategy.minimum_data_accumulation_seconds_before_trading < 0) {
        error_message = "strategy.minimum_data_accumulation_seconds_before_trading must be >= 0";
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
    
    // COMPLIANCE: No defaults allowed - explicit configuration required; fail hard if invalid
    // Validate maximum_dollar_value_per_trade is configured and > 0
    if (config.strategy.maximum_dollar_value_per_trade <= 0.0) {
        error_message = "Invalid configuration: risk.maximum_dollar_value_per_trade must be > 0.0 (provide via strategy_config.csv). Current value: " + std::to_string(config.strategy.maximum_dollar_value_per_trade);
        return false;
    }
    
    // Validate maximum_dollar_value_per_single_trade is configured and > 0
    if (config.strategy.maximum_dollar_value_per_single_trade <= 0.0) {
        error_message = "Invalid configuration: strategy.maximum_dollar_value_per_single_trade must be > 0.0 (provide via strategy_config.csv). Current value: " + std::to_string(config.strategy.maximum_dollar_value_per_single_trade);
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

    // Validate connectivity retry configuration
    if (config.timing.connectivity_max_retry_delay_seconds <= 0) {
        error_message = "timing.connectivity_max_retry_delay_seconds must be greater than 0";
        return false;
    }
    if (config.timing.connectivity_degraded_threshold <= 0) {
        error_message = "timing.connectivity_degraded_threshold must be greater than 0";
        return false;
    }
    if (config.timing.connectivity_disconnected_threshold <= 0) {
        error_message = "timing.connectivity_disconnected_threshold must be greater than 0";
        return false;
    }
    if (config.timing.connectivity_backoff_multiplier <= 1.0) {
        error_message = "timing.connectivity_backoff_multiplier must be greater than 1.0";
        return false;
    }
    if (config.timing.connectivity_disconnected_threshold <= config.timing.connectivity_degraded_threshold) {
        error_message = "timing.connectivity_disconnected_threshold must be greater than connectivity_degraded_threshold";
        return false;
    }

    // Validate system monitoring configuration (no defaults allowed)
    if (config.strategy.max_failure_rate_pct <= 0.0) {
        error_message = "strategy.max_failure_rate_pct must be configured and > 0.0 (no defaults allowed)";
        return false;
    }
    if (config.strategy.max_drawdown_pct <= 0.0) {
        error_message = "strategy.max_drawdown_pct must be configured and > 0.0 (no defaults allowed)";
        return false;
    }
    if (config.strategy.max_data_age_min <= 0) {
        error_message = "strategy.max_data_age_min must be configured and > 0 (no defaults allowed)";
        return false;
    }
    if (config.strategy.max_inactivity_min <= 0) {
        error_message = "strategy.max_inactivity_min must be configured and > 0 (no defaults allowed)";
        return false;
    }

    return true;
}

