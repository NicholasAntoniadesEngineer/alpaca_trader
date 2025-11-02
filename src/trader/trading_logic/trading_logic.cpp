#include "trading_logic.hpp"
#include "logging/logs/trading_logs.hpp"
#include "logging/logs/signal_analysis_logs.hpp"
#include "logging/logs/logger_structures.hpp"
#include "api/general/api_manager.hpp"
#include <chrono>
#include <cmath>
#include <memory>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;

TradingLogic::TradingLogic(const TradingLogicConstructionParams& construction_params)
    : config(construction_params.system_config), account_manager(construction_params.account_manager_ref), 
      api_manager(construction_params.api_manager_ref),
      risk_manager(construction_params.system_config),
      order_engine(OrderExecutionLogicConstructionParams(construction_params.api_manager_ref, construction_params.account_manager_ref, construction_params.system_config, nullptr)),
      market_data_manager(construction_params.system_config, construction_params.api_manager_ref, construction_params.account_manager_ref),
      connectivity_manager(construction_params.connectivity_manager_ref),
      data_sync_ptr(nullptr) {}

void TradingLogic::execute_trading_cycle(const MarketSnapshot& market_snapshot, const AccountSnapshot& account_snapshot, double initial_equity) {
    // Validate market snapshot
    if (!market_data_manager.get_market_data_validator().validate_market_snapshot(market_snapshot)) {
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
    if (!market_data_manager.is_data_fresh()) {
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
        double position_value_amount = trade_request.position_sizing.quantity * trade_request.processed_data.curr.close_price;
        double required_buying_power_amount = position_value_amount * config.strategy.buying_power_validation_safety_margin;
        TradingLogs::log_insufficient_buying_power(required_buying_power_amount, buying_power_amount, trade_request.position_sizing.quantity, trade_request.processed_data.curr.close_price);
        TradingLogs::log_trade_validation_failed("insufficient buying power");
        return;
    }
    
    TradingLogs::log_order_execution_header();
    
    bool is_long_position = trade_request.current_position_quantity > 0;
    bool is_short_position = trade_request.current_position_quantity < 0;
    TradingLogs::log_debug_position_data(trade_request.current_position_quantity, trade_request.processed_data.pos_details.current_value, trade_request.processed_data.pos_details.position_quantity, is_long_position, is_short_position);
    
    try {
        if (trade_request.signal_decision.buy) {
            TradingLogs::log_signal_triggered(config.strategy.signal_buy_string, true);
            
            if (trade_request.current_position_quantity == 0) {
                // Opening new position with bracket order - log exit targets before execution
                // Check if real-time price will be used
                double original_price = trade_request.processed_data.curr.close_price;
                ExitTargets exit_targets_result = order_engine.calculate_exit_targets(
                    OrderExecutionLogic::OrderSide::Buy, 
                    trade_request.processed_data, 
                    trade_request.position_sizing
                );
                
                // Log real-time price usage if configured
                if (config.strategy.use_current_market_price_for_order_execution) {
                    double realtime_price_amount = api_manager.get_current_price(config.trading_mode.primary_symbol);
                    if (realtime_price_amount > 0.0) {
                        TradingLogs::log_realtime_price_used(realtime_price_amount, original_price);
                    } else {
                        TradingLogs::log_realtime_price_fallback(original_price);
                    }
                }
                
                std::string order_side_string = config.strategy.signal_buy_string;
                Logging::ComprehensiveOrderExecutionRequest order_request_object("Bracket Order", order_side_string, trade_request.position_sizing.quantity, 
                                                                trade_request.processed_data.curr.close_price, trade_request.processed_data.atr, trade_request.processed_data.pos_details.position_quantity, 
                                                                trade_request.position_sizing.risk_amount, exit_targets_result.stop_loss, exit_targets_result.take_profit,
                                                                config.trading_mode.primary_symbol, "execute_bracket_order");
                TradingLogs::log_comprehensive_order_execution(order_request_object);
                
                Logging::ExitTargetsTableRequest exit_targets_request_object(order_side_string, trade_request.processed_data.curr.close_price, trade_request.position_sizing.risk_amount, config.strategy.rr_ratio, exit_targets_result.stop_loss, exit_targets_result.take_profit);
                TradingLogs::log_exit_targets_table(exit_targets_request_object);
    }
    
    order_engine.execute_trade(trade_request.processed_data, trade_request.current_position_quantity, trade_request.position_sizing, trade_request.signal_decision);
            TradingLogs::log_market_status(true, "BUY order executed successfully");
        } else if (trade_request.signal_decision.sell) {
            TradingLogs::log_signal_triggered(config.strategy.signal_sell_string, true);
            
            if (trade_request.current_position_quantity == 0) {
                if (!api_manager.get_account_info().empty()) {
                    TradingLogs::log_market_status(false, "SELL signal blocked - insufficient short availability for new position");
                    return;
                } else {
                    TradingLogs::log_market_status(true, "SELL signal - opening short position with bracket order");
                    double original_price = trade_request.processed_data.curr.close_price;
                    ExitTargets exit_targets_result = order_engine.calculate_exit_targets(
                        OrderExecutionLogic::OrderSide::Sell, 
                        trade_request.processed_data, 
                        trade_request.position_sizing
                    );
                    
                    // Log real-time price usage if configured
                    if (config.strategy.use_current_market_price_for_order_execution) {
                        double realtime_price_amount = api_manager.get_current_price(config.trading_mode.primary_symbol);
                        if (realtime_price_amount > 0.0) {
                            TradingLogs::log_realtime_price_used(realtime_price_amount, original_price);
                        } else {
                            TradingLogs::log_realtime_price_fallback(original_price);
                        }
                    }
                    
                    std::string order_side_string = config.strategy.signal_sell_string;
                    Logging::ComprehensiveOrderExecutionRequest order_request_object("Bracket Order", order_side_string, trade_request.position_sizing.quantity, 
                                                                    trade_request.processed_data.curr.close_price, trade_request.processed_data.atr, trade_request.processed_data.pos_details.position_quantity, 
                                                                    trade_request.position_sizing.risk_amount, exit_targets_result.stop_loss, exit_targets_result.take_profit,
                                                                    config.trading_mode.primary_symbol, "execute_bracket_order");
                    TradingLogs::log_comprehensive_order_execution(order_request_object);
                    
                    Logging::ExitTargetsTableRequest exit_targets_request_object(order_side_string, trade_request.processed_data.curr.close_price, trade_request.position_sizing.risk_amount, config.strategy.rr_ratio, exit_targets_result.stop_loss, exit_targets_result.take_profit);
                    TradingLogs::log_exit_targets_table(exit_targets_request_object);
                }
            } else if (trade_request.current_position_quantity > 0) {
                TradingLogs::log_market_status(true, "SELL signal - closing long position with market order");
                std::string order_side_string = config.strategy.signal_sell_string;
                Logging::ComprehensiveOrderExecutionRequest order_request_object("Market Order", order_side_string, trade_request.position_sizing.quantity,
                                                                    trade_request.processed_data.curr.close_price, trade_request.processed_data.atr, trade_request.processed_data.pos_details.position_quantity,
                                                                    trade_request.position_sizing.risk_amount, 0.0, 0.0,
                                                                    config.trading_mode.primary_symbol, "execute_market_order");
                TradingLogs::log_comprehensive_order_execution(order_request_object);
            } else {
                TradingLogs::log_market_status(true, "SELL signal - closing short position with market order");
                std::string order_side_string = config.strategy.signal_buy_string;
                Logging::ComprehensiveOrderExecutionRequest order_request_object("Market Order", order_side_string, trade_request.position_sizing.quantity,
                                                                    trade_request.processed_data.curr.close_price, trade_request.processed_data.atr, trade_request.processed_data.pos_details.position_quantity,
                                                                    trade_request.position_sizing.risk_amount, 0.0, 0.0,
                                                                    config.trading_mode.primary_symbol, "execute_market_order");
                TradingLogs::log_comprehensive_order_execution(order_request_object);
            }
            
            order_engine.execute_trade(trade_request.processed_data, trade_request.current_position_quantity, trade_request.position_sizing, trade_request.signal_decision);
            TradingLogs::log_market_status(true, "SELL order executed successfully");
        } else {
            TradingLogs::log_no_trading_pattern();
        }
    } catch (const std::runtime_error& runtime_error) {
        std::string error_message = runtime_error.what();
        if (error_message == "Order validation failed - aborting trade execution") {
            TradingLogs::log_trade_validation_failed("Order validation failed");
        } else if (error_message.find("Insufficient buying power") != std::string::npos) {
            double position_value_amount = trade_request.position_sizing.quantity * trade_request.processed_data.curr.close_price;
            double required_buying_power_amount = position_value_amount * config.strategy.buying_power_validation_safety_margin;
            TradingLogs::log_insufficient_buying_power(required_buying_power_amount, buying_power_amount, trade_request.position_sizing.quantity, trade_request.processed_data.curr.close_price);
        } else if (error_message.find("Position limits reached") != std::string::npos) {
            TradingLogs::log_position_limits_reached(trade_request.signal_decision.buy ? config.strategy.signal_buy_string : config.strategy.signal_sell_string);
        } else if (error_message.find("Wash trade prevention") != std::string::npos) {
            TradingLogs::log_market_status(false, error_message);
        } else if (error_message.find("Position closure failed") != std::string::npos) {
            TradingLogs::log_market_status(false, error_message);
        } else {
            TradingLogs::log_market_status(false, "Order execution error: " + error_message);
        }
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_status(false, "Order execution exception: " + std::string(exception_error.what()));
    } catch (...) {
        TradingLogs::log_market_status(false, "Unknown exception in order execution");
    }
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
        TradingLogs::log_position_closure("PROFIT TAKING THRESHOLD EXCEEDED", std::abs(current_position_quantity));
        Logging::ComprehensiveOrderExecutionRequest order_request_object("MARKET", "SELL", std::abs(current_position_quantity),
                                                       current_market_price, 0.0, current_position_quantity,
                                                       profit_threshold_dollars, 0.0, 0.0, "", "");
        TradingLogs::log_comprehensive_order_execution(order_request_object);
        
        PositionSizing profit_taking_position_sizing{std::abs(current_position_quantity), 0.0, 0.0, 0, 0, 0, 0};
        try {
        if (current_position_quantity > 0) {
            order_engine.execute_market_order(OrderExecutionLogic::OrderSide::Sell, processed_data_for_profit_taking, profit_taking_position_sizing);
        } else {
            order_engine.execute_market_order(OrderExecutionLogic::OrderSide::Buy, processed_data_for_profit_taking, profit_taking_position_sizing);
            }
            TradingLogs::log_market_status(true, "Profit taking order executed successfully");
        } catch (const std::exception& exception_error) {
            TradingLogs::log_market_status(false, "Profit taking order execution failed: " + std::string(exception_error.what()));
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
    
    int market_close_grace_period_minutes = config.timing.market_close_grace_period_minutes;
    TradingLogs::log_market_close_warning(market_close_grace_period_minutes);
    
    std::string order_side_string = (current_position_quantity > 0) ? config.strategy.signal_sell_string : config.strategy.signal_buy_string;
    TradingLogs::log_market_close_position_closure(current_position_quantity, config.trading_mode.primary_symbol, order_side_string);
    
    try {
    order_engine.handle_market_close_positions(processed_data_for_close);
        TradingLogs::log_market_status(true, "Market close position closure executed successfully");
        TradingLogs::log_market_close_complete();
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_status(false, "Market close position closure failed: " + std::string(exception_error.what()));
    }
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
