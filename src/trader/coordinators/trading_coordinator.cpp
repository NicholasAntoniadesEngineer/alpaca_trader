#include "trading_coordinator.hpp"
#include "logging/logs/trading_logs.hpp"
#include "logging/logs/signal_analysis_logs.hpp"
#include "logging/logs/market_data_logs.hpp"
#include "logging/logger/csv_trade_logger.hpp"
#include "logging/logs/logger_structures.hpp"
#include "utils/time_utils.hpp"
#include "trader/strategy_analysis/strategy_logic.hpp"
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::TradingLogs;
using AlpacaTrader::Logging::SignalAnalysisLogs;
using AlpacaTrader::Logging::MarketDataLogs;

TradingCoordinator::TradingCoordinator(TradingLogic& trading_logic_ref, MarketDataManager& market_data_manager_ref,
                                       ConnectivityManager& connectivity_manager_ref,
                                       AccountManager& account_manager_ref,
                                       const SystemConfig& system_config_param)
    : trading_logic(trading_logic_ref), market_data_manager(market_data_manager_ref), 
      connectivity_manager(connectivity_manager_ref), account_manager(account_manager_ref),
      config(system_config_param) {}

void TradingCoordinator::execute_trading_cycle_iteration(TradingSnapshotState& snapshot_state,
                                                          MarketDataSyncState& market_data_sync_state,
                                                          double initial_equity,
                                                          unsigned long loop_counter_value) {
    // Check connectivity status
    if (!connectivity_manager.check_connectivity_status()) {
        std::string connectivity_msg = "Connectivity outage - status: " + connectivity_manager.get_status_string();
        TradingLogs::log_market_status(false, connectivity_msg);
        try {
            trading_logic.handle_trading_halt();
        } catch (const std::exception& halt_exception_error) {
            TradingLogs::log_market_status(false, "Error handling trading halt: " + std::string(halt_exception_error.what()));
        } catch (...) {
            TradingLogs::log_market_status(false, "Unknown error handling trading halt");
        }
        return;
    }

    // Wait for fresh market data
    bool data_wait_result = false;
    try {
        data_wait_result = market_data_manager.wait_for_fresh_data(market_data_sync_state);
        if (data_wait_result) {
            MarketDataLogs::log_data_available(config.logging.log_file);
        }
    } catch (const std::runtime_error& runtime_error) {
        std::string error_message = runtime_error.what();
        if (error_message.find("Invalid sync state pointers") != std::string::npos) {
            MarketDataLogs::log_sync_state_error("Invalid sync state pointers", config.logging.log_file);
        } else if (error_message.find("Data availability wait failed") != std::string::npos) {
            MarketDataLogs::log_data_timeout(config.logging.log_file);
        } else {
            MarketDataLogs::log_sync_state_error(std::string("Runtime error in wait_for_fresh_data: ") + error_message, config.logging.log_file);
        }
        TradingLogs::log_market_status(false, "Failed to wait for fresh market data");
        return;
    } catch (const std::exception& exception_error) {
        MarketDataLogs::log_data_exception("Exception in wait_for_fresh_data: " + std::string(exception_error.what()), config.logging.log_file);
        TradingLogs::log_market_status(false, "Failed to wait for fresh market data");
        return;
    } catch (...) {
        MarketDataLogs::log_data_exception("Unknown exception in wait_for_fresh_data", config.logging.log_file);
        TradingLogs::log_market_status(false, "Failed to wait for fresh market data");
        return;
    }
    
    if (!data_wait_result) {
        TradingLogs::log_market_status(false, "Timeout waiting for fresh data");
        return;
    }
    
    if (!snapshot_state.running_flag.load()) {
        return;
    }

    // Lock and read snapshots atomically
    MarketSnapshot current_market_snapshot;
    AccountSnapshot current_account_snapshot;
    {
        std::lock_guard<std::mutex> state_lock(snapshot_state.state_mutex);
        current_market_snapshot = snapshot_state.market_snapshot;
        current_account_snapshot = snapshot_state.account_snapshot;
    }

    // Validate we have required data
    if (!snapshot_state.has_market_flag.load() || !snapshot_state.has_account_flag.load()) {
        TradingLogs::log_market_status(false, "Missing required snapshot data");
        return;
    }

    // Log loop header
    if (config.trading_mode.primary_symbol.empty()) {
        TradingLogs::log_market_status(false, "Primary symbol is required but not configured");
        return;
    }
    std::string symbol = config.trading_mode.primary_symbol;
    TradingLogs::log_loop_header(loop_counter_value, symbol);

    // CSV logging for account updates
    try {
        std::string timestamp = TimeUtils::get_current_human_readable_time();
        double buying_power = account_manager.fetch_buying_power();
        if (auto csv_trade_logger = AlpacaTrader::Logging::get_logging_context()->csv_trade_logger) {
            csv_trade_logger->log_account_update(
                timestamp, current_account_snapshot.equity, 
                buying_power, current_account_snapshot.exposure_pct
            );
        }
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_data_result_table("CSV logging error in account update: " + std::string(exception_error.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown CSV logging error in account update", false, 0);
    }

    // Check if market data is fresh before executing trading cycle
    bool market_data_fresh_result = false;
    try {
        market_data_fresh_result = market_data_manager.is_data_fresh();
        if (market_data_fresh_result) {
            TradingLogs::log_market_status(true, "Market data is fresh");
        } else {
            TradingLogs::log_market_status(false, "Market data is stale or unavailable");
        }
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_status(false, "Error checking market data freshness: " + std::string(exception_error.what()));
        return;
    } catch (...) {
        TradingLogs::log_market_status(false, "Unknown error checking market data freshness");
        return;
    }
    
    if (!market_data_fresh_result) {
        return;
    }
    
    // Create processed data for market close check
    ProcessedData processed_data_for_market_close(current_market_snapshot, current_account_snapshot);
    
    // Handle market close positions with logging
    try {
        API::ApiManager& api_manager_ref = market_data_manager.get_api_manager();
        bool market_open_result = api_manager_ref.is_market_open(config.trading_mode.primary_symbol);
        
        if (!market_open_result) {
            int position_quantity = processed_data_for_market_close.pos_details.position_quantity;
            if (position_quantity != 0) {
                // Log market close warning with grace period
                int grace_period_minutes = config.timing.market_close_grace_period_minutes;
                TradingLogs::log_market_close_warning(grace_period_minutes);
                
                // Determine position side for logging
                std::string position_side = (position_quantity > 0) ? config.strategy.position_long_string : config.strategy.position_short_string;
                TradingLogs::log_market_close_position_closure(position_quantity, config.trading_mode.primary_symbol, position_side);
                
                // Handle market close positions
                trading_logic.handle_market_close_positions(processed_data_for_market_close);
                
                TradingLogs::log_market_close_complete();
            }
        }
    } catch (const std::runtime_error& runtime_error) {
        TradingLogs::log_market_status(false, "Error handling market close positions: " + std::string(runtime_error.what()));
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_status(false, "Exception handling market close positions: " + std::string(exception_error.what()));
    } catch (...) {
        TradingLogs::log_market_status(false, "Unknown exception handling market close positions");
    }
    
    // Execute trading cycle and get decision result for logging
    TradingDecisionResult decision_result;
    try {
        decision_result = trading_logic.execute_trading_cycle(current_market_snapshot, current_account_snapshot, initial_equity);
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_status(false, "Error executing trading cycle: " + std::string(exception_error.what()));
        return;
    } catch (...) {
        TradingLogs::log_market_status(false, "Unknown error executing trading cycle");
        return;
    }
    
    // Log decision result based on outcome
    if (decision_result.validation_failed) {
        TradingLogs::log_market_status(false, decision_result.validation_error_message);
        return;
    }
    
    if (decision_result.market_closed) {
        TradingLogs::log_market_status(false, "Market is closed - outside trading hours - no trading decisions");
        TradingLogs::log_signal_analysis_complete();
        return;
    }
    
    TradingLogs::log_market_status(true, "Market is open - trading allowed");
    
    if (decision_result.market_data_stale) {
        TradingLogs::log_market_status(false, "Market data is stale - no trading decisions");
        TradingLogs::log_signal_analysis_complete();
        return;
    }
    
    // Log signal analysis start
    TradingLogs::log_signal_analysis_start(symbol);
    
    // Extract processed data for logging
    if (!decision_result.processed_data_ptr) {
        TradingLogs::log_market_status(false, "Invalid processed data pointer in decision result");
        return;
    }
    
    const ProcessedData& processed_data_for_logging = *decision_result.processed_data_ptr;
    
    // Log all signal analysis details
    TradingLogs::log_candle_data_table(processed_data_for_logging.curr.open_price, processed_data_for_logging.curr.high_price, 
                                       processed_data_for_logging.curr.low_price, processed_data_for_logging.curr.close_price);
    TradingLogs::log_signals_table_enhanced(decision_result.signal_decision);
    TradingLogs::log_signal_analysis_detailed(processed_data_for_logging, decision_result.signal_decision, config);
    TradingLogs::log_filters(decision_result.filter_result, config, processed_data_for_logging);
    TradingLogs::log_summary(processed_data_for_logging, decision_result.signal_decision, decision_result.filter_result, config.strategy.symbol);
    SignalAnalysisLogs::log_signal_analysis_csv_data(processed_data_for_logging, decision_result.signal_decision, decision_result.filter_result, config);
    
    // Log position sizing
    if (decision_result.filter_result.all_pass) {
        TradingLogs::log_filters_passed();
        TradingLogs::log_current_position(decision_result.current_position_quantity, config.strategy.symbol);
        TradingLogs::log_position_size_with_buying_power(decision_result.position_sizing_result.risk_amount, decision_result.position_sizing_result.quantity, 
                                                        decision_result.buying_power_amount, processed_data_for_logging.curr.close_price);
        TradingLogs::log_position_sizing_debug(decision_result.position_sizing_result.risk_based_qty, decision_result.position_sizing_result.exposure_based_qty, 
                                               decision_result.position_sizing_result.max_value_qty, decision_result.position_sizing_result.buying_power_qty, 
                                               decision_result.position_sizing_result.quantity);
    } else {
        TradingLogs::log_filters_not_met_preview(decision_result.position_sizing_result.risk_amount, decision_result.position_sizing_result.quantity);
    }
    
    TradingLogs::log_position_sizing_csv(decision_result.position_sizing_result, processed_data_for_logging, config, decision_result.buying_power_amount);
    
    // Execute trade if decision indicates we should
    if (decision_result.should_execute_trade && decision_result.processed_data_ptr) {
        TradeExecutionRequest trade_request(*decision_result.processed_data_ptr, decision_result.current_position_quantity, 
                                          decision_result.position_sizing_result, decision_result.signal_decision);
        
        // Execute trade with comprehensive logging
        log_and_execute_trade_with_comprehensive_logging(trade_request, decision_result);
    } else {
        TradingLogs::log_no_trading_pattern();
    }
    
    TradingLogs::log_signal_analysis_complete();
}

MarketDataManager& TradingCoordinator::get_market_data_manager_reference() {
    return market_data_manager;
}

void TradingCoordinator::process_trading_cycle_iteration(MarketSnapshot& market_snapshot,
                                                         AccountSnapshot& account_snapshot,
                                                         std::mutex& state_mtx,
                                                         std::condition_variable& data_cv,
                                                         std::atomic<bool>& has_market,
                                                         std::atomic<bool>& has_account,
                                                         std::atomic<bool>& running,
                                                         std::atomic<std::chrono::steady_clock::time_point>& market_data_timestamp,
                                                         std::atomic<bool>& market_data_fresh,
                                                         std::atomic<std::chrono::steady_clock::time_point>& last_order_timestamp,
                                                         std::atomic<bool>* allow_fetch_ptr,
                                                         double initial_equity,
                                                         std::atomic<unsigned long>& loop_counter) {
    try {
        // Increment loop counter
        unsigned long current_loop_counter = loop_counter.fetch_add(1) + 1;
        
        // Create snapshot state structure
        TradingSnapshotState snapshot_state{
            market_snapshot,
            account_snapshot,
            state_mtx,
            data_cv,
            has_market,
            has_account,
            running
        };
        
        // Create market data sync state structure
        MarketDataSyncState market_data_sync_state(
            &state_mtx,
            &data_cv,
            &market_snapshot,
            &account_snapshot,
            &has_market,
            &has_account,
            &running,
            allow_fetch_ptr ? allow_fetch_ptr : &running,  // Use allow_fetch if available, otherwise running
            &market_data_timestamp,
            &market_data_fresh,
            &last_order_timestamp
        );
        
        // Execute trading cycle iteration through coordinator
        execute_trading_cycle_iteration(snapshot_state, market_data_sync_state, initial_equity, current_loop_counter);
        
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_data_result_table("Exception in process_trading_cycle_iteration: " + std::string(exception_error.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown exception in process_trading_cycle_iteration", false, 0);
    }
}

void TradingCoordinator::countdown_to_next_cycle(std::atomic<bool>& running, int poll_interval_sec, int countdown_refresh_interval_sec) {
    int sleep_secs = poll_interval_sec;
    int countdown_refresh_interval = countdown_refresh_interval_sec;
    
    // If countdown refresh interval is 0 or greater than sleep_secs, just sleep once
    if (countdown_refresh_interval <= 0 || countdown_refresh_interval >= sleep_secs) {
        if (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(sleep_secs));
        }
        return;
    }
    
    // Calculate how many countdown updates we should show
    int num_updates = sleep_secs / countdown_refresh_interval;
    int remaining_secs = sleep_secs;
    
    for (int update_index = 0; update_index < num_updates && running.load(); ++update_index) {
        int display_secs = std::min(remaining_secs, countdown_refresh_interval);
        TradingLogs::log_inline_next_loop(display_secs);
        
        if (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(countdown_refresh_interval));
            remaining_secs -= countdown_refresh_interval;
        }
    }
    
    // Sleep any remaining time without countdown display
    if (remaining_secs > 0 && running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(remaining_secs));
    }
    
    TradingLogs::end_inline_status();
}

void TradingCoordinator::log_and_execute_trade_with_comprehensive_logging(const TradeExecutionRequest& trade_request, const TradingDecisionResult& decision_result) {
    if (trade_request.position_sizing.quantity < 1) {
        TradingLogs::log_position_sizing_skipped("quantity < 1");
        return;
    }
    
    double buying_power_amount = decision_result.buying_power_amount;
    OrderExecutionLogic& order_engine = trading_logic.get_order_engine();
    
    // Validate trade feasibility
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
    TradingLogs::log_debug_position_data(trade_request.current_position_quantity, trade_request.processed_data.pos_details.current_value, 
                                         trade_request.processed_data.pos_details.position_quantity, is_long_position, is_short_position);
    
    try {
        if (trade_request.signal_decision.buy) {
            TradingLogs::log_signal_triggered(config.strategy.signal_buy_string, true);
            
            if (trade_request.current_position_quantity == 0) {
                double original_price = trade_request.processed_data.curr.close_price;
                ExitTargets exit_targets_result = order_engine.calculate_exit_targets(
                    OrderExecutionLogic::OrderSide::Buy, 
                    trade_request.processed_data, 
                    trade_request.position_sizing
                );
                
                if (config.strategy.use_current_market_price_for_order_execution) {
                    API::ApiManager& api_manager_ref = market_data_manager.get_api_manager();
                    double realtime_price_amount = api_manager_ref.get_current_price(config.trading_mode.primary_symbol);
                    if (realtime_price_amount > 0.0) {
                        TradingLogs::log_realtime_price_used(realtime_price_amount, original_price);
                    } else {
                        TradingLogs::log_realtime_price_fallback(original_price);
                    }
                }
                
                std::string order_side_string = config.strategy.signal_buy_string;
                Logging::ComprehensiveOrderExecutionRequest order_request_object("Bracket Order", order_side_string, trade_request.position_sizing.quantity, 
                                                                trade_request.processed_data.curr.close_price, trade_request.processed_data.atr, 
                                                                trade_request.processed_data.pos_details.position_quantity, 
                                                                trade_request.position_sizing.risk_amount, exit_targets_result.stop_loss, 
                                                                exit_targets_result.take_profit, config.trading_mode.primary_symbol, "execute_bracket_order");
                TradingLogs::log_comprehensive_order_execution(order_request_object);
                
                Logging::ExitTargetsTableRequest exit_targets_request_object(order_side_string, trade_request.processed_data.curr.close_price, 
                                                                             trade_request.position_sizing.risk_amount, config.strategy.rr_ratio, 
                                                                             exit_targets_result.stop_loss, exit_targets_result.take_profit);
                TradingLogs::log_exit_targets_table(exit_targets_request_object);
            }
            
            trading_logic.execute_trade_if_valid(trade_request);
            TradingLogs::log_market_status(true, "BUY order executed successfully");
        } else if (trade_request.signal_decision.sell) {
            TradingLogs::log_signal_triggered(config.strategy.signal_sell_string, true);
            
            if (trade_request.current_position_quantity == 0) {
                API::ApiManager& api_manager_ref = market_data_manager.get_api_manager();
                if (!api_manager_ref.get_account_info().empty()) {
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
                    
                    if (config.strategy.use_current_market_price_for_order_execution) {
                        double realtime_price_amount = api_manager_ref.get_current_price(config.trading_mode.primary_symbol);
                        if (realtime_price_amount > 0.0) {
                            TradingLogs::log_realtime_price_used(realtime_price_amount, original_price);
                        } else {
                            TradingLogs::log_realtime_price_fallback(original_price);
                        }
                    }
                    
                    std::string order_side_string = config.strategy.signal_sell_string;
                    Logging::ComprehensiveOrderExecutionRequest order_request_object("Bracket Order", order_side_string, trade_request.position_sizing.quantity, 
                                                                    trade_request.processed_data.curr.close_price, trade_request.processed_data.atr, 
                                                                    trade_request.processed_data.pos_details.position_quantity, 
                                                                    trade_request.position_sizing.risk_amount, exit_targets_result.stop_loss, 
                                                                    exit_targets_result.take_profit, config.trading_mode.primary_symbol, "execute_bracket_order");
                    TradingLogs::log_comprehensive_order_execution(order_request_object);
                    
                    Logging::ExitTargetsTableRequest exit_targets_request_object(order_side_string, trade_request.processed_data.curr.close_price, 
                                                                                trade_request.position_sizing.risk_amount, config.strategy.rr_ratio, 
                                                                                exit_targets_result.stop_loss, exit_targets_result.take_profit);
                    TradingLogs::log_exit_targets_table(exit_targets_request_object);
                }
            } else if (trade_request.current_position_quantity > 0) {
                TradingLogs::log_market_status(true, "SELL signal - closing long position with market order");
                std::string order_side_string = config.strategy.signal_sell_string;
                Logging::ComprehensiveOrderExecutionRequest order_request_object("Market Order", order_side_string, trade_request.position_sizing.quantity,
                                                                    trade_request.processed_data.curr.close_price, trade_request.processed_data.atr, 
                                                                    trade_request.processed_data.pos_details.position_quantity,
                                                                    trade_request.position_sizing.risk_amount, 0.0, 0.0,
                                                                    config.trading_mode.primary_symbol, "execute_market_order");
                TradingLogs::log_comprehensive_order_execution(order_request_object);
            } else {
                TradingLogs::log_market_status(true, "SELL signal - closing short position with market order");
                std::string order_side_string = config.strategy.signal_buy_string;
                Logging::ComprehensiveOrderExecutionRequest order_request_object("Market Order", order_side_string, trade_request.position_sizing.quantity,
                                                                    trade_request.processed_data.curr.close_price, trade_request.processed_data.atr, 
                                                                    trade_request.processed_data.pos_details.position_quantity,
                                                                    trade_request.position_sizing.risk_amount, 0.0, 0.0,
                                                                    config.trading_mode.primary_symbol, "execute_market_order");
                TradingLogs::log_comprehensive_order_execution(order_request_object);
            }
            
            trading_logic.execute_trade_if_valid(trade_request);
            TradingLogs::log_market_status(true, "SELL order executed successfully");
        } else {
            TradingLogs::log_no_trading_pattern();
        }
    } catch (const std::runtime_error& runtime_error) {
        log_trade_execution_error(runtime_error.what(), trade_request, buying_power_amount);
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_status(false, "Order execution exception: " + std::string(exception_error.what()));
    } catch (...) {
        TradingLogs::log_market_status(false, "Unknown exception in order execution");
    }
}

void TradingCoordinator::log_trade_execution_error(const std::string& error_message, const TradeExecutionRequest& trade_request, double buying_power_amount) {
    if (error_message == "Order validation failed - aborting trade execution") {
        TradingLogs::log_trade_validation_failed("Order validation failed");
    } else if (error_message.find("Insufficient buying power") != std::string::npos || error_message.find("insufficient buying power") != std::string::npos) {
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
}

} // namespace Core
} // namespace AlpacaTrader
