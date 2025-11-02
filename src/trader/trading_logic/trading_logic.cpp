#include "trading_logic.hpp"
#include "api/general/api_manager.hpp"
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>

namespace AlpacaTrader {
namespace Core {

TradingLogic::TradingLogic(const TradingLogicConstructionParams& construction_params)
    : config(construction_params.system_config), account_manager(construction_params.account_manager_ref), 
      api_manager(construction_params.api_manager_ref),
      risk_manager(construction_params.system_config),
      order_engine(OrderExecutionLogicConstructionParams(construction_params.api_manager_ref, construction_params.account_manager_ref, construction_params.system_config, nullptr)),
      market_data_manager(construction_params.system_config, construction_params.api_manager_ref, construction_params.account_manager_ref),
      connectivity_manager(construction_params.connectivity_manager_ref),
      data_sync_ptr(nullptr) {}

TradingDecisionResult TradingLogic::execute_trading_cycle(const MarketSnapshot& market_snapshot, const AccountSnapshot& account_snapshot, double initial_equity) {
    TradingDecisionResult empty_result;
    
    // Validate market snapshot
    if (!market_data_manager.get_market_data_validator().validate_market_snapshot(market_snapshot)) {
        return empty_result;
    }
    
    // Create processed data from snapshots
    ProcessedData processed_data_for_trading(market_snapshot, account_snapshot);
    
    // Validate trading permissions
    if (!risk_manager.validate_trading_permissions(ProcessedData{}, account_snapshot.equity, initial_equity)) {
        handle_trading_halt();
        return empty_result;
    }
    
    // Execute trading decision (trade execution and logging handled by coordinator)
    TradingDecisionResult decision_result = execute_trading_decision(processed_data_for_trading, account_snapshot.equity);
    
    return decision_result;
}

TradingDecisionResult TradingLogic::execute_trading_decision(const ProcessedData& processed_data_input, double account_equity) {
    TradingDecisionResult result;
    
    // Input validation
    if (config.trading_mode.primary_symbol.empty()) {
        result.validation_failed = true;
        result.validation_error_message = "Invalid configuration - primary symbol is empty";
        return result;
    }

    if (account_equity <= 0.0 || !std::isfinite(account_equity)) {
        result.validation_failed = true;
        result.validation_error_message = "Invalid equity value - must be positive and finite";
        return result;
    }

    // Check if market is open before making any trading decisions
    if (!api_manager.is_within_trading_hours(config.trading_mode.primary_symbol)) {
        result.market_closed = true;
        return result;
    }
    
    // Check if market data is fresh enough for trading decisions
    if (!market_data_manager.is_data_fresh()) {
        result.market_data_stale = true;
        return result;
    }
    
    int current_position_quantity = processed_data_input.pos_details.position_quantity;

    if (current_position_quantity != 0 && config.strategy.profit_taking_threshold_dollars > 0.0) {
        ProfitTakingRequest profit_taking_request(processed_data_input, current_position_quantity, config.strategy.profit_taking_threshold_dollars);
        check_and_execute_profit_taking(profit_taking_request);
    }

    result.signal_decision = detect_trading_signals(processed_data_input, config);
    result.filter_result = evaluate_trading_filters(processed_data_input, config);

    result.buying_power_amount = account_manager.fetch_buying_power();
    auto [position_sizing_result, position_sizing_signal_decision] = AlpacaTrader::Core::process_position_sizing(PositionSizingProcessRequest(
        processed_data_input, account_equity, current_position_quantity, result.buying_power_amount, config.strategy, config.trading_mode
    ));
    
    result.position_sizing_result = position_sizing_result;
    result.processed_data_ptr = &processed_data_input;
    result.current_position_quantity = current_position_quantity;
    
    if (position_sizing_result.quantity >= 1) {
        result.should_execute_trade = true;
    }
    
    return result;
}

void TradingLogic::handle_trading_halt() {
    ConnectivityManager& connectivity_manager_ref = connectivity_manager;
    
    int halt_seconds_amount = 0;
    if (connectivity_manager_ref.is_connectivity_outage()) {
        halt_seconds_amount = connectivity_manager_ref.get_seconds_until_retry();
        if (halt_seconds_amount <= 0) {
            int emergency_trading_halt_duration_seconds = config.timing.emergency_trading_halt_duration_minutes * 60;
            if (emergency_trading_halt_duration_seconds <= 0) {
                throw std::runtime_error("Invalid emergency trading halt duration");
            }
            halt_seconds_amount = emergency_trading_halt_duration_seconds;
        }
    } else {
        halt_seconds_amount = config.timing.emergency_trading_halt_duration_minutes * 60;
        if (halt_seconds_amount <= 0) {
            throw std::runtime_error("Invalid emergency trading halt duration");
        }
    }
    
    perform_halt_countdown(halt_seconds_amount);
}

void TradingLogic::execute_trade_if_valid(const TradeExecutionRequest& trade_request) {
    if (trade_request.position_sizing.quantity < 1) {
        return;
    }
    
    double buying_power_amount = account_manager.fetch_buying_power();
    if (!order_engine.validate_trade_feasibility(trade_request.position_sizing, buying_power_amount, trade_request.processed_data.curr.close_price)) {
        throw std::runtime_error("Insufficient buying power for trade");
    }
    
    if (trade_request.signal_decision.buy) {
        if (trade_request.current_position_quantity == 0) {
            if (!api_manager.get_account_info().empty()) {
                throw std::runtime_error("SELL signal blocked - insufficient short availability for new position");
            }
        }
        order_engine.execute_trade(trade_request.processed_data, trade_request.current_position_quantity, trade_request.position_sizing, trade_request.signal_decision);
    } else if (trade_request.signal_decision.sell) {
        order_engine.execute_trade(trade_request.processed_data, trade_request.current_position_quantity, trade_request.position_sizing, trade_request.signal_decision);
    }
}

void TradingLogic::perform_halt_countdown(int halt_duration_seconds) const {
    for (int remaining_seconds_count = halt_duration_seconds; remaining_seconds_count > 0; --remaining_seconds_count) {
        std::this_thread::sleep_for(std::chrono::seconds(config.timing.countdown_display_refresh_interval_seconds));
    }
}

void TradingLogic::check_and_execute_profit_taking(const ProfitTakingRequest& profit_taking_request) {
    const ProcessedData& processed_data_for_profit_taking = profit_taking_request.processed_data;
    int current_position_quantity = profit_taking_request.current_position_quantity;
    double profit_threshold_dollars = profit_taking_request.profit_taking_threshold_dollars;
    
    double unrealized_profit_loss = processed_data_for_profit_taking.pos_details.unrealized_pl;

    if (unrealized_profit_loss > profit_threshold_dollars) {
        PositionSizing profit_taking_position_sizing{std::abs(current_position_quantity), 0.0, 0.0, 0, 0, 0, 0};
        if (current_position_quantity > 0) {
            order_engine.execute_market_order(OrderExecutionLogic::OrderSide::Sell, processed_data_for_profit_taking, profit_taking_position_sizing);
        } else {
            order_engine.execute_market_order(OrderExecutionLogic::OrderSide::Buy, processed_data_for_profit_taking, profit_taking_position_sizing);
        }
    }
}

void TradingLogic::handle_market_close_positions(const ProcessedData& processed_data_for_close) {
    if (api_manager.is_market_open(config.trading_mode.primary_symbol)) {
        return;
    }
    
    int current_position_quantity = processed_data_for_close.pos_details.position_quantity;
    if (current_position_quantity == 0) {
        return;
    }
    
    order_engine.handle_market_close_positions(processed_data_for_close);
}

MarketDataManager& TradingLogic::get_market_data_manager_reference() {
    return market_data_manager;
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
