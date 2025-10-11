#include "trading_engine.hpp"
#include "core/trader/analysis/risk_logic.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/logging_macros.hpp"
#include "core/logging/market_data_logs.hpp"
#include "core/system/system_monitor.hpp"
#include <chrono>
#include <cmath>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

TradingEngine::TradingEngine(const SystemConfig& cfg, API::AlpacaClient& client_ref, AccountManager& account_mgr)
    : config(cfg), account_manager(account_mgr), client(client_ref),
      order_engine(client_ref, account_mgr, cfg, data_sync),
      position_manager(client_ref, cfg),
      trade_validator(cfg),
      price_manager(client_ref, cfg),
      data_fetcher(client_ref, account_mgr, cfg) {}

bool TradingEngine::check_trading_permissions(const ProcessedData& data, double equity) {
    if (!check_connectivity()) {
        return false;
    }
    
    return validate_risk_conditions(data, equity);
}

void TradingEngine::execute_trading_decision(const ProcessedData& data, double equity) {
    // Input validation
    if (config.strategy.symbol.empty()) {
        TradingLogs::log_market_status(false, "Invalid configuration - symbol is empty");
        return;
    }

    if (equity <= 0.0 || !std::isfinite(equity)) {
        TradingLogs::log_market_status(false, "Invalid equity value");
        return;
    }

    TradingLogs::log_signal_analysis_start(config.strategy.symbol);

    // Check if market is open before making any trading decisions
    if (!is_market_open()) {
        TradingLogs::log_market_status(false, "Market is closed - no trading decisions");
        TradingLogs::log_signal_analysis_complete();
        return;
    }
    
    // Check if market data is fresh enough for trading decisions
    if (!is_data_fresh()) {
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

bool TradingEngine::validate_risk_conditions(const ProcessedData& data, double equity) {
    RiskLogic::TradeGateInput in;
    in.initial_equity = 0.0;
    in.current_equity = equity;
    in.exposure_pct = data.exposure_pct;
    
    RiskLogic::TradeGateResult res = RiskLogic::evaluate_trade_gate(in, config);
    
    bool trading_allowed = res.pnl_ok && res.exposure_ok;
    TradingLogs::log_trading_conditions(res.daily_pnl, data.exposure_pct, trading_allowed, config);
    
    if (!res.pnl_ok || !res.exposure_ok) {
        return false;
    }
    
    TradingLogs::log_market_status(true);
    return true;
}

bool TradingEngine::validate_market_data(const MarketSnapshot& market) const {
    // Check if this is a default/empty MarketSnapshot (no data available)
    if (market.atr == 0.0 && market.avg_atr == 0.0 && market.avg_vol == 0.0 && 
        market.curr.o == 0.0 && market.curr.h == 0.0 && market.curr.l == 0.0 && market.curr.c == 0.0) {
        AlpacaTrader::Logging::MarketDataLogs::log_market_data_failure_summary(
            config.strategy.symbol,
            "No Data Available",
            "Symbol may not exist or market is closed",
            0,
            config.logging.log_file
        );
        return false;
    }

    // Validate that market data is not null/empty
    if (std::isnan(market.curr.c) || std::isnan(market.atr) || !std::isfinite(market.curr.c) || !std::isfinite(market.atr)) {
        AlpacaTrader::Logging::MarketDataLogs::log_market_data_failure_summary(
            config.strategy.symbol,
            "Invalid Data",
            "NaN or infinite values detected in market data",
            0,
            config.logging.log_file
        );
        return false;
    }

    if (market.curr.c <= 0.0) {
        AlpacaTrader::Logging::MarketDataLogs::log_market_data_failure_summary(
            config.strategy.symbol,
            "Invalid Data",
            "Price is zero or negative",
            0,
            config.logging.log_file
        );
        return false;
    }

    if (market.atr <= 0.0) {
        AlpacaTrader::Logging::MarketDataLogs::log_market_data_failure_summary(
            config.strategy.symbol,
            "Insufficient Data",
            "ATR is zero or negative - insufficient volatility data for trading",
            0,
            config.logging.log_file
        );
        return false;
    }

    // Validate OHLC data is reasonable (H >= L, H >= C, L <= C)
    if (market.curr.h < market.curr.l || market.curr.h < market.curr.c || market.curr.l > market.curr.c) {
        AlpacaTrader::Logging::MarketDataLogs::log_market_data_failure_summary(
            config.strategy.symbol,
            "Invalid Data",
            "OHLC relationship violation - invalid price data structure",
            0,
            config.logging.log_file
        );
        return false;
    }

    return true;
}

bool TradingEngine::check_connectivity() {
    auto& connectivity = ConnectivityManager::instance();
    if (connectivity.is_connectivity_outage()) {
        std::string connectivity_msg = "Connectivity outage - status: " + connectivity.get_status_string();
        TradingLogs::log_market_status(false, connectivity_msg);
        return false;
    }
    return true;
}

bool TradingEngine::is_market_open() {
    if (!client.is_within_fetch_window()) {
        TradingLogs::log_market_status(false, "Market is closed - outside trading hours");
        return false;
    }
    TradingLogs::log_market_status(true, "Market is open - trading allowed");
    return true;
}

bool TradingEngine::is_data_fresh() {
    // Check if data_sync references are properly initialized
    if (!data_sync.market_data_timestamp) {
        TradingLogs::log_market_status(false, "Data sync not initialized - market_data_timestamp is null");
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto data_timestamp = data_sync.market_data_timestamp->load();
    
    // Use crypto-specific staleness threshold for 24/7 markets
    auto max_age_seconds = config.strategy.is_crypto_asset ? 
        config.timing.crypto_data_staleness_threshold_seconds : // Crypto: use crypto-specific threshold
        config.timing.market_data_staleness_threshold_seconds; // Stocks: use standard threshold

    // Additional null check before using the timestamp (defensive programming)
    if (!data_sync.market_data_timestamp) {
        TradingLogs::log_market_status(false, "Data sync lost during execution - market_data_timestamp became null");
        return false;
    }
    
    if (data_timestamp == std::chrono::steady_clock::time_point::min()) {
        TradingLogs::log_market_status(false, "Market data timestamp is invalid - no data received yet");
        return false;
    }
    
    auto age_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - data_timestamp).count();
    bool fresh = age_seconds <= max_age_seconds;
    
    if (!fresh) {
        TradingLogs::log_market_status(false, "Market data is stale - age: " + std::to_string(age_seconds) + 
                                    "s, max: " + std::to_string(max_age_seconds) + "s");
    } else {
        TradingLogs::log_market_status(true, "Market data is fresh - age: " + std::to_string(age_seconds) + "s");
    }
    
    return fresh;
}

void TradingEngine::setup_data_synchronization(const DataSyncConfig& config) {
    data_sync = DataSyncReferences(config);
    TradingLogs::log_market_status(true, "Data synchronization setup completed - market_data_timestamp initialized");
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
}

void TradingEngine::process_position_sizing(const ProcessedData& data, double equity, int current_qty) {
    double buying_power = account_manager.fetch_buying_power();
    StrategyLogic::PositionSizing sizing = StrategyLogic::calculate_position_sizing(data, equity, current_qty, config, buying_power);
    
    if (!StrategyLogic::evaluate_trading_filters(data, config).all_pass) {
        TradingLogs::log_filters_not_met_preview(sizing.risk_amount, sizing.quantity);
        return;
    }
    
    TradingLogs::log_filters_passed();
    TradingLogs::log_current_position(current_qty, config.strategy.symbol);
    TradingLogs::log_position_size_with_buying_power(sizing.risk_amount, sizing.quantity, buying_power, data.curr.c);
    TradingLogs::log_position_sizing_debug(sizing.risk_based_qty, sizing.exposure_based_qty, sizing.max_value_qty, sizing.buying_power_qty, sizing.quantity);
    
    StrategyLogic::SignalDecision signal_decision = StrategyLogic::detect_trading_signals(data, config);
    execute_trade_if_valid(data, current_qty, sizing, signal_decision);
}

void TradingEngine::execute_trade_if_valid(const ProcessedData& data, int current_qty, const StrategyLogic::PositionSizing& sizing, const StrategyLogic::SignalDecision& signal_decision) {
    if (sizing.quantity < 1) {
        TradingLogs::log_position_sizing_skipped("quantity < 1");
        return;
    }
    
    double buying_power = account_manager.fetch_buying_power();
    if (!trade_validator.validate_trade_feasibility(sizing, buying_power, data.curr.c)) {
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

void TradingEngine::handle_market_close_positions(const ProcessedData& data) {
    position_manager.handle_market_close_positions(data);
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
