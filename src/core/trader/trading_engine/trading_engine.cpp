#include "trading_engine.hpp"
#include "core/trader/analysis/risk_logic.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/logging_macros.hpp"
#include "core/logging/market_data_logs.hpp"
#include "core/logging/csv_bars_logger.hpp"
#include "core/logging/csv_trade_logger.hpp"
#include "core/system/system_monitor.hpp"
#include "core/utils/time_utils.hpp"
#include <chrono>
#include <cmath>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

TradingEngine::TradingEngine(const SystemConfig& cfg, API::ApiManager& api_mgr, AccountManager& account_mgr)
    : config(cfg), account_manager(account_mgr),
      risk_manager(cfg),
      order_engine(api_mgr, account_mgr, cfg, data_sync),
      price_manager(api_mgr, cfg),
      data_fetcher(api_mgr, account_mgr, cfg) {}

bool TradingEngine::check_trading_permissions(const ProcessedData& data, double equity) {
    if (!ConnectivityManager::instance().check_connectivity()) {
        return false;
    }
    
    return risk_manager.validate_risk_conditions(data, equity);
}

void TradingEngine::execute_trading_decision(const ProcessedData& data, double equity) {
    // Input validation
    if (config.trading_mode.primary_symbol.empty()) {
        TradingLogs::log_market_status(false, "Invalid configuration - primary symbol is empty");
        return;
    }

    if (equity <= 0.0 || !std::isfinite(equity)) {
        TradingLogs::log_market_status(false, "Invalid equity value");
        return;
    }

    TradingLogs::log_signal_analysis_start(config.trading_mode.primary_symbol);

    // Check if market is open before making any trading decisions
    if (!data_fetcher.is_market_open()) {
        TradingLogs::log_market_status(false, "Market is closed - no trading decisions");
        TradingLogs::log_signal_analysis_complete();
        return;
    }
    
    // Check if market data is fresh enough for trading decisions
    if (!data_fetcher.is_data_fresh()) {
        TradingLogs::log_market_status(false, "Market data is stale - no trading decisions");
        AlpacaTrader::Core::Monitoring::SystemMonitor::instance().record_data_freshness_failure();
        TradingLogs::log_signal_analysis_complete();
        return;
    }
    
    int current_qty = data.pos_details.qty;
    
    // Check for profit-taking opportunity first
    if (current_qty != 0 && config.strategy.profit_taking_threshold_dollars > 0.0) {
        check_and_execute_profit_taking(data, current_qty);
    }
    
    // Process signal analysis
    process_signal_analysis(data);
    
    // Process position sizing and execute if valid
    process_position_sizing(data, equity, current_qty);
    
    TradingLogs::log_signal_analysis_complete();
}

void TradingEngine::handle_trading_halt(const std::string& reason) {
    TradingLogs::log_market_status(false, reason);
    
    auto& connectivity = ConnectivityManager::instance();
    
    int halt_seconds = 0;
    if (connectivity.is_connectivity_outage()) {
        halt_seconds = connectivity.get_seconds_until_retry();
        if (halt_seconds <= 0) {
            halt_seconds = CONNECTIVITY_RETRY_SECONDS;
        }
    } else {
        halt_seconds = config.timing.emergency_trading_halt_duration_minutes * 60;
    }
    
    perform_halt_countdown(halt_seconds);
    TradingLogs::end_inline_status();
}

void TradingEngine::process_signal_analysis(const ProcessedData& data) {
    StrategyLogic::SignalDecision signal_decision = StrategyLogic::detect_trading_signals(data, config);

    // Log candle data and enhanced signals table
    TradingLogs::log_candle_data_table(data.curr.o, data.curr.h, data.curr.l, data.curr.c);
    TradingLogs::log_signals_table_enhanced(signal_decision);

    // Enhanced detailed signal analysis logging
    TradingLogs::log_signal_analysis_detailed(data, signal_decision, config);

    StrategyLogic::FilterResult filter_result = StrategyLogic::evaluate_trading_filters(data, config);
    TradingLogs::log_filters(filter_result, config, data);
    TradingLogs::log_summary(data, signal_decision, filter_result, config.strategy.symbol);

    // CSV logging for signal analysis
    try {
        std::string timestamp = TimeUtils::get_current_human_readable_time();
        if (config.trading_mode.primary_symbol.empty()) {
            throw std::runtime_error("Primary symbol is required but not configured");
        }
        std::string symbol = config.trading_mode.primary_symbol;

        // Log signals to CSV
        if (AlpacaTrader::Logging::g_csv_trade_logger) {
            AlpacaTrader::Logging::g_csv_trade_logger->log_signal(
                timestamp, symbol, signal_decision.buy, signal_decision.sell,
                signal_decision.signal_strength, signal_decision.signal_reason
            );
        }

        // Log filters to CSV
        if (AlpacaTrader::Logging::g_csv_trade_logger) {
            AlpacaTrader::Logging::g_csv_trade_logger->log_filters(
                timestamp, symbol, filter_result.atr_pass, filter_result.atr_ratio,
                config.strategy.use_absolute_atr_threshold ?
                    config.strategy.atr_absolute_minimum_threshold :
                    config.strategy.entry_signal_atr_multiplier,
                filter_result.vol_pass, filter_result.vol_ratio,
                filter_result.doji_pass
            );
        }

        // Log market data to CSV
        if (AlpacaTrader::Logging::g_csv_trade_logger) {
            AlpacaTrader::Logging::g_csv_trade_logger->log_market_data(
                timestamp, symbol, data.curr.o, data.curr.h, data.curr.l, data.curr.c,
                data.curr.v, data.atr
            );
        }

    } catch (const std::exception& e) {
        TradingLogs::log_market_data_result_table("CSV logging error in signal analysis: " + std::string(e.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown CSV logging error in signal analysis", false, 0);
    }
}

void TradingEngine::process_position_sizing(const ProcessedData& data, double equity, int current_qty) {
    double buying_power = account_manager.fetch_buying_power();
    StrategyLogic::PositionSizing sizing = StrategyLogic::calculate_position_sizing(data, equity, current_qty, config, buying_power);

    if (!StrategyLogic::evaluate_trading_filters(data, config).all_pass) {
        TradingLogs::log_filters_not_met_preview(sizing.risk_amount, sizing.quantity);

        // CSV logging for position sizing when filters not met
        try {
            std::string timestamp = TimeUtils::get_current_human_readable_time();
            if (config.trading_mode.primary_symbol.empty()) {
            throw std::runtime_error("Primary symbol is required but not configured");
        }
        std::string symbol = config.trading_mode.primary_symbol;

            if (AlpacaTrader::Logging::g_csv_trade_logger) {
                AlpacaTrader::Logging::g_csv_trade_logger->log_position_sizing(
                    timestamp, symbol, sizing.quantity, sizing.risk_amount,
                    sizing.quantity * data.curr.c, buying_power
                );
            }
        } catch (const std::exception& e) {
            TradingLogs::log_market_data_result_table("CSV logging error in position sizing: " + std::string(e.what()), false, 0);
        } catch (...) {
            TradingLogs::log_market_data_result_table("Unknown CSV logging error in position sizing", false, 0);
        }
        return;
    }
    
    TradingLogs::log_filters_passed();
    TradingLogs::log_current_position(current_qty, config.strategy.symbol);
    TradingLogs::log_position_size_with_buying_power(sizing.risk_amount, sizing.quantity, buying_power, data.curr.c);
    TradingLogs::log_position_sizing_debug(sizing.risk_based_qty, sizing.exposure_based_qty, sizing.max_value_qty, sizing.buying_power_qty, sizing.quantity);

    StrategyLogic::SignalDecision signal_decision = StrategyLogic::detect_trading_signals(data, config);
    execute_trade_if_valid(data, current_qty, sizing, signal_decision);

    // CSV logging for successful position sizing
    try {
        std::string timestamp = TimeUtils::get_current_human_readable_time();
        if (config.trading_mode.primary_symbol.empty()) {
            throw std::runtime_error("Primary symbol is required but not configured");
        }
        std::string symbol = config.trading_mode.primary_symbol;

        if (AlpacaTrader::Logging::g_csv_trade_logger) {
            AlpacaTrader::Logging::g_csv_trade_logger->log_position_sizing(
                timestamp, symbol, sizing.quantity, sizing.risk_amount,
                sizing.quantity * data.curr.c, buying_power
            );
        }
    } catch (const std::exception& e) {
        TradingLogs::log_market_data_result_table("CSV logging error in successful position sizing: " + std::string(e.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown CSV logging error in successful position sizing", false, 0);
    }
}

void TradingEngine::execute_trade_if_valid(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& signal_decision) {
    if (sizing.quantity < 1) {
        TradingLogs::log_position_sizing_skipped("quantity < 1");
        return;
    }
    
    double buying_power = account_manager.fetch_buying_power();
    if (!order_engine.validate_trade_feasibility(sizing, buying_power, data.curr.c)) {
        TradingLogs::log_trade_validation_failed("insufficient buying power");
        return;
    }
    
    order_engine.execute_trade(data, current_qty, sizing, signal_decision);
}

void TradingEngine::perform_halt_countdown(int seconds) const {
    for (int s = seconds; s > 0; --s) {
        TradingLogs::log_inline_halt_status(s);
        std::this_thread::sleep_for(std::chrono::seconds(config.timing.countdown_display_refresh_interval_seconds));
    }
}

void TradingEngine::check_and_execute_profit_taking(const ProcessedData& data, int current_qty) {
    // Get unrealized profit/loss from position details
    double unrealized_pl = data.pos_details.unrealized_pl;
    double current_price = data.curr.c;
    double position_value = data.pos_details.current_value;
    
    // Log profit calculation using proper TradingLogs class
    AlpacaTrader::Logging::TradingLogs::log_position_sizing_debug(current_qty, position_value, current_qty, true, false);
    AlpacaTrader::Logging::TradingLogs::log_exit_targets_table("LONG", current_price, config.strategy.profit_taking_threshold_dollars,
                                                           config.strategy.rr_ratio, 0.0, 0.0);

    // Check if profit exceeds threshold
    if (unrealized_pl > config.strategy.profit_taking_threshold_dollars) {
        AlpacaTrader::Logging::TradingLogs::log_position_closure("PROFIT TAKING THRESHOLD EXCEEDED", abs(current_qty));
        AlpacaTrader::Logging::TradingLogs::log_comprehensive_order_execution("MARKET", "SELL", abs(current_qty),
                                                                              current_price, 0.0, current_qty,
                                                                              config.strategy.profit_taking_threshold_dollars);
        
        // Execute market order to close entire position
        if (current_qty > 0) {
            // Long position: sell to close
            order_engine.execute_market_order(OrderExecutionEngine::OrderSide::Sell, data, 
                                            StrategyLogic::PositionSizing{abs(current_qty), 0.0, 0.0, static_cast<int>(0)});
        } else {
            // Short position: buy to close
            order_engine.execute_market_order(OrderExecutionEngine::OrderSide::Buy, data, 
                                            StrategyLogic::PositionSizing{abs(current_qty), 0.0, 0.0, static_cast<int>(0)});
        }
    }
}

} // namespace Core
} // namespace AlpacaTrader
