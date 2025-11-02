#include "trading_logic.hpp"
#include "logging/logs/trading_logs.hpp"
#include "logging/logs/signal_analysis_logs.hpp"
#include "logging/logs/logger_structures.hpp"
#include "utils/time_utils.hpp"
#include "api/general/api_manager.hpp"
#include <chrono>
#include <cmath>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

TradingLogic::TradingLogic(const TradingLogicConstructionParams& construction_params)
    : config(construction_params.system_config), account_manager(construction_params.account_manager_ref), 
      api_manager(construction_params.api_manager_ref),
      risk_manager(construction_params.system_config),
      order_engine(OrderExecutionLogicConstructionParams(construction_params.api_manager_ref, construction_params.account_manager_ref, construction_params.system_config, nullptr)),
      data_fetcher(construction_params.api_manager_ref, construction_params.account_manager_ref, construction_params.system_config),
      connectivity_manager(construction_params.connectivity_manager_ref),
      data_sync_ptr(nullptr) {}

void TradingLogic::execute_trading_cycle(const MarketSnapshot& market_snapshot, const AccountSnapshot& account_snapshot, double initial_equity) {
    // Validate market snapshot
    if (!data_fetcher.get_market_data_validator().validate_market_snapshot(market_snapshot)) {
        return;
    }
    
    // Create processed data from snapshots
    ProcessedData processed_data_for_trading(market_snapshot, account_snapshot);
    
    // Handle market close positions
    handle_market_close_positions(processed_data_for_trading);
    
    // Validate trading permissions
    if (!risk_manager.validate_trading_permissions(ProcessedData{}, account_snapshot.equity, initial_equity)) {
        handle_trading_halt("Trading conditions not met");
        return;
    }
    
    // Execute trading decision
    execute_trading_decision(processed_data_for_trading, account_snapshot.equity);
}

void TradingLogic::execute_trading_decision(const ProcessedData& processed_data_input, double account_equity) {
    // Input validation
    if (config.trading_mode.primary_symbol.empty()) {
        TradingLogs::log_market_status(false, "Invalid configuration - primary symbol is empty");
        return;
    }

    if (account_equity <= 0.0 || !std::isfinite(account_equity)) {
        TradingLogs::log_market_status(false, "Invalid equity value - must be positive and finite");
        return;
    }

    TradingLogs::log_signal_analysis_start(config.trading_mode.primary_symbol);

    // Check if market is open before making any trading decisions
    if (!api_manager.is_within_trading_hours(config.trading_mode.primary_symbol)) {
        TradingLogs::log_market_status(false, "Market is closed - outside trading hours - no trading decisions");
        TradingLogs::log_signal_analysis_complete();
        return;
    }
    TradingLogs::log_market_status(true, "Market is open - trading allowed");
    
    // Check if market data is fresh enough for trading decisions
    if (!data_fetcher.is_data_fresh()) {
        TradingLogs::log_market_status(false, "Market data is stale - no trading decisions");
        TradingLogs::log_signal_analysis_complete();
        return;
    }
    
    int current_position_quantity = processed_data_input.pos_details.position_quantity;

    if (current_position_quantity != 0 && config.strategy.profit_taking_threshold_dollars > 0.0) {
        ProfitTakingRequest profit_taking_request(processed_data_input, current_position_quantity, config.strategy.profit_taking_threshold_dollars);
        check_and_execute_profit_taking(profit_taking_request);
    }

    SignalDecision signal_decision_result = detect_trading_signals(processed_data_input, config);
    FilterResult filter_result_output = evaluate_trading_filters(processed_data_input, config);

    TradingLogs::log_candle_data_table(processed_data_input.curr.open_price, processed_data_input.curr.high_price, processed_data_input.curr.low_price, processed_data_input.curr.close_price);
    TradingLogs::log_signals_table_enhanced(signal_decision_result);
    TradingLogs::log_signal_analysis_detailed(processed_data_input, signal_decision_result, config);
    TradingLogs::log_filters(filter_result_output, config, processed_data_input);
    TradingLogs::log_summary(processed_data_input, signal_decision_result, filter_result_output, config.strategy.symbol);
    AlpacaTrader::Logging::SignalAnalysisLogs::log_signal_analysis_csv_data(processed_data_input, signal_decision_result, filter_result_output, config);

    double buying_power_amount = account_manager.fetch_buying_power();
    auto [position_sizing_result, position_sizing_signal_decision] = AlpacaTrader::Core::process_position_sizing(PositionSizingProcessRequest(
        processed_data_input, account_equity, current_position_quantity, buying_power_amount, config.strategy, config.trading_mode
    ));
    
    if (filter_result_output.all_pass) {
        TradingLogs::log_filters_passed();
        TradingLogs::log_current_position(current_position_quantity, config.strategy.symbol);
        TradingLogs::log_position_size_with_buying_power(position_sizing_result.risk_amount, position_sizing_result.quantity, buying_power_amount, processed_data_input.curr.close_price);
        TradingLogs::log_position_sizing_debug(position_sizing_result.risk_based_qty, position_sizing_result.exposure_based_qty, position_sizing_result.max_value_qty, position_sizing_result.buying_power_qty, position_sizing_result.quantity);
    } else {
        TradingLogs::log_filters_not_met_preview(position_sizing_result.risk_amount, position_sizing_result.quantity);
    }
    
    TradingLogs::log_position_sizing_csv(position_sizing_result, processed_data_input, config, buying_power_amount);
    
    TradeExecutionRequest trade_request(processed_data_input, current_position_quantity, position_sizing_result, signal_decision_result);
    execute_trade_if_valid(trade_request);
    
    TradingLogs::log_signal_analysis_complete();
}

void TradingLogic::handle_trading_halt(const std::string& reason) {
    TradingLogs::log_market_status(false, reason);
    
    ConnectivityManager& connectivity_manager_ref = connectivity_manager;
    
    int halt_seconds_amount = 0;
    if (connectivity_manager_ref.is_connectivity_outage()) {
        halt_seconds_amount = connectivity_manager_ref.get_seconds_until_retry();
        if (halt_seconds_amount <= 0) {
            int emergency_trading_halt_duration_seconds = config.timing.emergency_trading_halt_duration_minutes * 60;
            if (emergency_trading_halt_duration_seconds <= 0) {
                TradingLogs::log_market_status(false, "Invalid emergency trading halt duration");
                return;
            }
            halt_seconds_amount = emergency_trading_halt_duration_seconds;
        }
    } else {
        halt_seconds_amount = config.timing.emergency_trading_halt_duration_minutes * 60;
        if (halt_seconds_amount <= 0) {
            TradingLogs::log_market_status(false, "Invalid emergency trading halt duration");
            return;
        }
    }
    
    perform_halt_countdown(halt_seconds_amount);
    TradingLogs::end_inline_status();
}

void TradingLogic::execute_trade_if_valid(const TradeExecutionRequest& trade_request) {
    if (trade_request.position_sizing.quantity < 1) {
        TradingLogs::log_position_sizing_skipped("quantity < 1");
        return;
    }
    
    double buying_power_amount = account_manager.fetch_buying_power();
    if (!order_engine.validate_trade_feasibility(trade_request.position_sizing, buying_power_amount, trade_request.processed_data.curr.close_price)) {
        TradingLogs::log_trade_validation_failed("insufficient buying power");
        return;
    }
    
    order_engine.execute_trade(trade_request.processed_data, trade_request.current_position_quantity, trade_request.position_sizing, trade_request.signal_decision);
}

void TradingLogic::perform_halt_countdown(int halt_duration_seconds) const {
    for (int remaining_seconds_count = halt_duration_seconds; remaining_seconds_count > 0; --remaining_seconds_count) {
        TradingLogs::log_inline_halt_status(remaining_seconds_count);
        std::this_thread::sleep_for(std::chrono::seconds(config.timing.countdown_display_refresh_interval_seconds));
    }
}

void TradingLogic::check_and_execute_profit_taking(const ProfitTakingRequest& profit_taking_request) {
    const ProcessedData& processed_data_for_profit_taking = profit_taking_request.processed_data;
    int current_position_quantity = profit_taking_request.current_position_quantity;
    double profit_threshold_dollars = profit_taking_request.profit_taking_threshold_dollars;
    
    double unrealized_profit_loss = processed_data_for_profit_taking.pos_details.unrealized_pl;
    double current_market_price = processed_data_for_profit_taking.curr.close_price;
    double current_position_value = processed_data_for_profit_taking.pos_details.current_value;
    
    TradingLogs::log_position_sizing_debug(current_position_quantity, current_position_value, current_position_quantity, true, false);
    Logging::ExitTargetsTableRequest exit_targets_request_object("LONG", current_market_price, profit_threshold_dollars, config.strategy.rr_ratio, 0.0, 0.0);
    TradingLogs::log_exit_targets_table(exit_targets_request_object);

    if (unrealized_profit_loss > profit_threshold_dollars) {
        TradingLogs::log_position_closure("PROFIT TAKING THRESHOLD EXCEEDED", abs(current_position_quantity));
        Logging::ComprehensiveOrderExecutionRequest order_request_object("MARKET", "SELL", abs(current_position_quantity),
                                                       current_market_price, 0.0, current_position_quantity,
                                                       profit_threshold_dollars, 0.0, 0.0, "", "");
        TradingLogs::log_comprehensive_order_execution(order_request_object);
        
        PositionSizing profit_taking_position_sizing{abs(current_position_quantity), 0.0, 0.0, 0, 0, 0, 0};
        if (current_position_quantity > 0) {
            order_engine.execute_market_order(OrderExecutionLogic::OrderSide::Sell, processed_data_for_profit_taking, profit_taking_position_sizing);
        } else {
            order_engine.execute_market_order(OrderExecutionLogic::OrderSide::Buy, processed_data_for_profit_taking, profit_taking_position_sizing);
        }
    }
}

void TradingLogic::handle_market_close_positions(const ProcessedData& processed_data_for_close) {
    order_engine.handle_market_close_positions(processed_data_for_close);
}

MarketDataFetcher& TradingLogic::get_market_data_fetcher_reference() {
    return data_fetcher;
}

void TradingLogic::setup_data_synchronization(const DataSyncConfig& sync_configuration) {
    if (data_sync_ptr) {
        throw std::runtime_error("Data synchronization already initialized");
    }
    
    data_sync_ptr = std::make_unique<DataSyncReferences>(sync_configuration);
    order_engine.set_data_sync_reference(data_sync_ptr.get());
    
    if (!data_sync_ptr->mtx || !data_sync_ptr->cv || !data_sync_ptr->market || !data_sync_ptr->account || 
        !data_sync_ptr->has_market || !data_sync_ptr->has_account || !data_sync_ptr->running || !data_sync_ptr->allow_fetch) {
        throw std::runtime_error("Invalid data sync configuration: one or more required pointers are null");
    }
}

} // namespace Core
} // namespace AlpacaTrader
