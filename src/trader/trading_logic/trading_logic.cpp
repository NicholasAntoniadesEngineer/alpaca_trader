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
    
    try {
        
        // Validate market snapshot
        try {
            bool validation_result = market_data_manager.get_market_data_validator().validate_market_snapshot(market_snapshot);
            
            if (!validation_result) {
                return empty_result;
            }
        } catch (const std::exception& validation_exception_error) {
            empty_result.validation_failed = true;
            empty_result.validation_error_message = "Exception validating market snapshot: " + std::string(validation_exception_error.what());
            return empty_result;
        } catch (...) {
            empty_result.validation_failed = true;
            empty_result.validation_error_message = "Unknown exception validating market snapshot";
            return empty_result;
        }
        
        
        // Check if sufficient data accumulation time has elapsed before allowing trades
        if (!market_snapshot.oldest_bar_timestamp.empty()) {
            try {
                long long oldest_bar_timestamp_millis_value = std::stoll(market_snapshot.oldest_bar_timestamp);
                if (oldest_bar_timestamp_millis_value > 0) {
                    auto current_time_millis_value = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count();
                    
                    long long data_accumulation_time_seconds_value = (current_time_millis_value - oldest_bar_timestamp_millis_value) / 1000LL;
                    int minimum_accumulation_seconds_value = config.strategy.minimum_data_accumulation_seconds_before_trading;
                    
                    if (data_accumulation_time_seconds_value < minimum_accumulation_seconds_value) {
                        empty_result.validation_failed = true;
                        empty_result.validation_error_message = "Insufficient data accumulation time. Accumulated: " + std::to_string(data_accumulation_time_seconds_value) + " seconds. Required: " + std::to_string(minimum_accumulation_seconds_value) + " seconds.";
                        return empty_result;
                    }
                }
            } catch (const std::exception& accumulation_check_exception_error) {
                // CRITICAL: Fail hard - do not silently allow trading on exception
                empty_result.validation_failed = true;
                empty_result.validation_error_message = "CRITICAL: Exception in data accumulation check: " + std::string(accumulation_check_exception_error.what());
                return empty_result;
            } catch (...) {
                // CRITICAL: Fail hard - do not silently allow trading on exception
                empty_result.validation_failed = true;
                empty_result.validation_error_message = "CRITICAL: Unknown exception in data accumulation check";
                return empty_result;
            }
        }
        
        
        // CRITICAL: Validate snapshots before creating ProcessedData
        try {
            // Validate market snapshot bar data
            
            if (!std::isfinite(market_snapshot.curr.close_price) || !std::isfinite(market_snapshot.curr.open_price) ||
                !std::isfinite(market_snapshot.curr.high_price) || !std::isfinite(market_snapshot.curr.low_price)) {
                empty_result.validation_failed = true;
                empty_result.validation_error_message = "CRITICAL: Invalid market snapshot curr bar data - non-finite prices";
                return empty_result;
            }
            
            // Validate prev bar if it exists (might be empty during initial accumulation)
            if (market_snapshot.prev.close_price > 0.0 || market_snapshot.prev.open_price > 0.0) {
                if (!std::isfinite(market_snapshot.prev.close_price) || !std::isfinite(market_snapshot.prev.open_price) ||
                    !std::isfinite(market_snapshot.prev.high_price) || !std::isfinite(market_snapshot.prev.low_price)) {
                    empty_result.validation_failed = true;
                    empty_result.validation_error_message = "CRITICAL: Invalid market snapshot prev bar data - non-finite prices";
                    return empty_result;
                }
            }
            
            // Validate ATR values
            if (!std::isfinite(market_snapshot.atr)) {
                empty_result.validation_failed = true;
                empty_result.validation_error_message = "CRITICAL: Invalid market snapshot ATR - non-finite value";
                return empty_result;
            }
            
            // Validate account snapshot
            if (!std::isfinite(account_snapshot.equity)) {
                empty_result.validation_failed = true;
                empty_result.validation_error_message = "CRITICAL: Invalid account snapshot equity - non-finite value";
                return empty_result;
            }
        } catch (const std::exception& snapshot_validation_exception_error) {
            empty_result.validation_failed = true;
            empty_result.validation_error_message = "CRITICAL: Exception validating snapshots: " + std::string(snapshot_validation_exception_error.what());
            return empty_result;
        } catch (...) {
            empty_result.validation_failed = true;
            empty_result.validation_error_message = "CRITICAL: Unknown exception validating snapshots";
            return empty_result;
        }
        
        
        // Create processed data from snapshots
        ProcessedData processed_data_for_trading;
        try {
            
            processed_data_for_trading = ProcessedData(market_snapshot, account_snapshot);
            
            
            // CRITICAL: Validate ProcessedData after creation
            
            if (!std::isfinite(processed_data_for_trading.curr.close_price) || !std::isfinite(processed_data_for_trading.curr.open_price)) {
                empty_result.validation_failed = true;
                empty_result.validation_error_message = "CRITICAL: ProcessedData created with invalid curr bar data";
                return empty_result;
            }
            
            
            if (!std::isfinite(processed_data_for_trading.atr)) {
                empty_result.validation_failed = true;
                empty_result.validation_error_message = "CRITICAL: ProcessedData created with invalid ATR";
                return empty_result;
            }
            
        } catch (const std::exception& processed_data_exception_error) {
            empty_result.validation_failed = true;
            empty_result.validation_error_message = "CRITICAL: Exception creating ProcessedData: " + std::string(processed_data_exception_error.what());
            return empty_result;
        } catch (...) {
            empty_result.validation_failed = true;
            empty_result.validation_error_message = "CRITICAL: Unknown exception creating ProcessedData";
            return empty_result;
        }
        
        
        // Validate trading permissions - CRITICAL: Use actual processed_data, not empty
        try {
            
            bool permissions_result = risk_manager.validate_trading_permissions(processed_data_for_trading, account_snapshot.equity, initial_equity);
            
            
            if (!permissions_result) {
                
                // NOTE: Instead of returning early, we'll continue to execute the trading decision
                // so we can see the logs. The trading execution will be blocked by other checks.
                // We'll set a flag to indicate permissions were denied but still proceed.
                
                // Continue execution - trading decision will still run but won't execute trades
                // The coordinator will handle the permissions check result
            } else {
            }
        } catch (const std::exception& permissions_exception_error) {
            empty_result.validation_failed = true;
            empty_result.validation_error_message = "CRITICAL: Exception validating trading permissions: " + std::string(permissions_exception_error.what());
            return empty_result;
        } catch (...) {
            empty_result.validation_failed = true;
            empty_result.validation_error_message = "CRITICAL: Unknown exception validating trading permissions";
            return empty_result;
        }
        
        
        // CRITICAL: Final validation before executing trading decision
        try {
            
            if (!std::isfinite(processed_data_for_trading.curr.close_price) || processed_data_for_trading.curr.close_price <= 0.0) {
                empty_result.validation_failed = true;
                empty_result.validation_error_message = "CRITICAL: Invalid close price before trading decision: " + std::to_string(processed_data_for_trading.curr.close_price);
                return empty_result;
            }
            
            if (!std::isfinite(account_snapshot.equity) || account_snapshot.equity <= 0.0) {
                empty_result.validation_failed = true;
                empty_result.validation_error_message = "CRITICAL: Invalid equity before trading decision: " + std::to_string(account_snapshot.equity);
                return empty_result;
            }
            
        } catch (const std::exception& final_validation_exception_error) {
            empty_result.validation_failed = true;
            empty_result.validation_error_message = "CRITICAL: Exception in final validation: " + std::string(final_validation_exception_error.what());
            return empty_result;
        } catch (...) {
            empty_result.validation_failed = true;
            empty_result.validation_error_message = "CRITICAL: Unknown exception in final validation";
            return empty_result;
        }
        
        
        // Execute trading decision (trade execution and logging handled by coordinator)
        TradingDecisionResult decision_result;
        try {
            
            decision_result = execute_trading_decision(processed_data_for_trading, account_snapshot.equity);
            
        } catch (const std::exception& trading_decision_exception_error) {
            empty_result.validation_failed = true;
            empty_result.validation_error_message = "CRITICAL: Exception executing trading decision: " + std::string(trading_decision_exception_error.what());
            return empty_result;
        } catch (...) {
            empty_result.validation_failed = true;
            empty_result.validation_error_message = "CRITICAL: Unknown exception executing trading decision";
            return empty_result;
        }
        
        
        return decision_result;
    } catch (const std::exception& execute_cycle_exception_error) {
        empty_result.validation_failed = true;
        empty_result.validation_error_message = "Exception in execute_trading_cycle: " + std::string(execute_cycle_exception_error.what());
        return empty_result;
    } catch (...) {
        empty_result.validation_failed = true;
        empty_result.validation_error_message = "Unknown exception in execute_trading_cycle";
        return empty_result;
    }
}

TradingDecisionResult TradingLogic::execute_trading_decision(const ProcessedData& processed_data_input, double account_equity) {
    
    TradingDecisionResult result;
    
    // Top-level try-catch for entire function
    try {
        
    // Input validation
    try {
        if (config.trading_mode.primary_symbol.empty()) {
            result.validation_failed = true;
            result.validation_error_message = "Invalid configuration - primary symbol is empty";
            return result;
        }
    } catch (const std::exception& config_access_exception_error) {
        result.validation_failed = true;
        result.validation_error_message = "Exception accessing config primary symbol: " + std::string(config_access_exception_error.what());
        return result;
    } catch (...) {
        result.validation_failed = true;
        result.validation_error_message = "Unknown exception accessing config primary symbol";
        return result;
    }

    
    try {
        
        if (account_equity <= 0.0 || !std::isfinite(account_equity)) {
            result.validation_failed = true;
            result.validation_error_message = "Invalid equity value - must be positive and finite";
            return result;
        }
        
    } catch (const std::exception& equity_check_exception_error) {
        result.validation_failed = true;
        result.validation_error_message = "Exception checking equity value: " + std::string(equity_check_exception_error.what());
        return result;
    } catch (...) {
        result.validation_failed = true;
        result.validation_error_message = "Unknown exception checking equity value";
        return result;
    }

    
    // Check if market is open before making any trading decisions
    try {
        std::string primary_symbol;
        try {
            primary_symbol = config.trading_mode.primary_symbol;
        } catch (const std::exception& symbol_access_exception_error) {
            result.validation_failed = true;
            result.validation_error_message = "Exception accessing primary symbol: " + std::string(symbol_access_exception_error.what());
            return result;
        } catch (...) {
            result.validation_failed = true;
            result.validation_error_message = "Unknown exception accessing primary symbol";
            return result;
        }
        
        try {
            
            bool market_open = api_manager.is_within_trading_hours(primary_symbol);
            
            
            if (!market_open) {
                result.market_closed = true;
                return result;
            }
            
        } catch (const std::exception& trading_hours_api_exception_error) {
            result.validation_failed = true;
            result.validation_error_message = "API error checking trading hours: " + std::string(trading_hours_api_exception_error.what());
            return result;
        } catch (...) {
            result.validation_failed = true;
            result.validation_error_message = "Unknown API error checking trading hours";
            return result;
        }
    } catch (const std::exception& market_open_check_exception_error) {
        result.validation_failed = true;
        result.validation_error_message = "Exception in market open check: " + std::string(market_open_check_exception_error.what());
        return result;
    } catch (...) {
        result.validation_failed = true;
        result.validation_error_message = "Unknown exception in market open check";
        return result;
    }
    
    
    // Check if market data is fresh enough for trading decisions
    bool data_fresh = false;
    try {
        
        data_fresh = market_data_manager.is_data_fresh();
        
    } catch (const std::exception& data_fresh_exception_error) {
        result.validation_failed = true;
        result.validation_error_message = "Exception checking data freshness: " + std::string(data_fresh_exception_error.what());
        return result;
    } catch (...) {
        result.validation_failed = true;
        result.validation_error_message = "Unknown exception checking data freshness";
        return result;
    }
    
    if (!data_fresh) {
        result.market_data_stale = true;
        return result;
    }
    
    
    int current_position_quantity = 0;
    try {
        
        current_position_quantity = processed_data_input.pos_details.position_quantity;
        
    } catch (const std::exception& position_quantity_exception_error) {
        result.validation_failed = true;
        result.validation_error_message = "Exception accessing position quantity: " + std::string(position_quantity_exception_error.what());
        return result;
    } catch (...) {
        result.validation_failed = true;
        result.validation_error_message = "Unknown exception accessing position quantity";
        return result;
    }


    try {
        if (current_position_quantity != 0 && config.strategy.profit_taking_threshold_dollars > 0.0) {
            try {
                ProfitTakingRequest profit_taking_request(processed_data_input, current_position_quantity, config.strategy.profit_taking_threshold_dollars);
                check_and_execute_profit_taking(profit_taking_request);
            } catch (const std::exception& profit_taking_exception_error) {
                // Log but continue - profit taking failed
            } catch (...) {
                // Unknown exception in profit taking - continue
            }
        } else {
        }
    } catch (const std::exception& profit_taking_check_exception_error) {
        // Log but continue - profit taking check failed
    } catch (...) {
        // Unknown exception in profit taking check - continue
    }


    try {
        result.signal_decision = detect_trading_signals(processed_data_input, config);
    } catch (const std::exception& signal_detection_exception_error) {
        result.validation_failed = true;
        result.validation_error_message = "Exception detecting trading signals: " + std::string(signal_detection_exception_error.what());
        return result;
    } catch (...) {
        result.validation_failed = true;
        result.validation_error_message = "Unknown exception detecting trading signals";
        return result;
    }
    
    
    try {
        result.filter_result = evaluate_trading_filters(processed_data_input, config);
    } catch (const std::exception& filter_evaluation_exception_error) {
        result.validation_failed = true;
        result.validation_error_message = "Exception evaluating trading filters: " + std::string(filter_evaluation_exception_error.what());
        return result;
    } catch (...) {
        result.validation_failed = true;
        result.validation_error_message = "Unknown exception evaluating trading filters";
        return result;
    }


    try {
        result.buying_power_amount = account_manager.fetch_buying_power();
    } catch (const std::exception& buying_power_exception_error) {
        result.validation_failed = true;
        result.validation_error_message = "Exception fetching buying power: " + std::string(buying_power_exception_error.what());
        return result;
    } catch (...) {
        result.validation_failed = true;
        result.validation_error_message = "Unknown exception fetching buying power";
        return result;
    }
    
    
    try {
        
        auto [position_sizing_result, position_sizing_signal_decision] = AlpacaTrader::Core::process_position_sizing(PositionSizingProcessRequest(
            processed_data_input, account_equity, current_position_quantity, result.buying_power_amount, config.strategy, config.trading_mode
        ));
        
        
        result.position_sizing_result = position_sizing_result;
        // CRITICAL: Store copy of ProcessedData instead of pointer to avoid use-after-free
        // Explicitly copy all fields to ensure complete data transfer
        result.processed_data.atr = processed_data_input.atr;
        result.processed_data.avg_atr = processed_data_input.avg_atr;
        result.processed_data.avg_vol = processed_data_input.avg_vol;
        result.processed_data.pos_details = processed_data_input.pos_details;
        result.processed_data.open_orders = processed_data_input.open_orders;
        result.processed_data.exposure_pct = processed_data_input.exposure_pct;
        result.processed_data.is_doji = processed_data_input.is_doji;
        result.processed_data.oldest_bar_timestamp = processed_data_input.oldest_bar_timestamp;
        
        // Explicitly copy curr Bar fields
        result.processed_data.curr.open_price = processed_data_input.curr.open_price;
        result.processed_data.curr.high_price = processed_data_input.curr.high_price;
        result.processed_data.curr.low_price = processed_data_input.curr.low_price;
        result.processed_data.curr.close_price = processed_data_input.curr.close_price;
        result.processed_data.curr.volume = processed_data_input.curr.volume;
        result.processed_data.curr.timestamp = processed_data_input.curr.timestamp;
        
        // Explicitly copy prev Bar fields
        result.processed_data.prev.open_price = processed_data_input.prev.open_price;
        result.processed_data.prev.high_price = processed_data_input.prev.high_price;
        result.processed_data.prev.low_price = processed_data_input.prev.low_price;
        result.processed_data.prev.close_price = processed_data_input.prev.close_price;
        result.processed_data.prev.volume = processed_data_input.prev.volume;
        result.processed_data.prev.timestamp = processed_data_input.prev.timestamp;
        
        result.current_position_quantity = current_position_quantity;
        
        if (position_sizing_result.quantity > 0.0) {
            result.should_execute_trade = true;
        } else {
        }
    } catch (const std::exception& position_sizing_exception_error) {
        result.validation_failed = true;
        result.validation_error_message = "Exception in position sizing: " + std::string(position_sizing_exception_error.what());
        return result;
    } catch (...) {
        result.validation_failed = true;
        result.validation_error_message = "Unknown exception in position sizing";
        return result;
    }
    
    
    return result;
    } catch (const std::exception& top_level_decision_exception_error) {
        result.validation_failed = true;
        result.validation_error_message = "CRITICAL: Top-level exception in execute_trading_decision: " + std::string(top_level_decision_exception_error.what());
        return result;
    } catch (...) {
        result.validation_failed = true;
        result.validation_error_message = "CRITICAL: Unknown top-level exception in execute_trading_decision - segfault prevented";
        return result;
    }
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
    if (trade_request.position_sizing.quantity <= 0.0) {
        return;
    }
    
    double buying_power_amount = account_manager.fetch_buying_power();
    if (!order_engine.validate_trade_feasibility(trade_request.position_sizing, buying_power_amount, trade_request.processed_data.curr.close_price)) {
        throw std::runtime_error("Insufficient buying power for trade");
    }
    
    // For crypto (BTC/USD), shorts are always available - no availability check needed
    // Execute trade directly for both buy and sell signals
    order_engine.execute_trade(trade_request.processed_data, trade_request.current_position_quantity, trade_request.position_sizing, trade_request.signal_decision);
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
        PositionSizing profit_taking_position_sizing{static_cast<double>(std::abs(current_position_quantity)), 0.0, 0.0, 0, 0, 0, 0};
        if (current_position_quantity > 0) {
            order_engine.execute_market_order(OrderExecutionLogic::OrderSide::Sell, processed_data_for_profit_taking, profit_taking_position_sizing);
        } else {
            order_engine.execute_market_order(OrderExecutionLogic::OrderSide::Buy, processed_data_for_profit_taking, profit_taking_position_sizing);
        }
    }
}

bool TradingLogic::handle_market_close_positions(const ProcessedData& processed_data_for_close) {
    try {
        if (api_manager.is_market_open(config.trading_mode.primary_symbol)) {
            return false;
        }
    } catch (const std::exception& market_open_api_exception_error) {
        // If API check fails, assume market is closed and proceed with position closure
        // Error will be logged by coordinator
    } catch (...) {
        // If unknown error, assume market is closed and proceed with position closure
    }
    
    int current_position_quantity = processed_data_for_close.pos_details.position_quantity;
    if (current_position_quantity == 0) {
        return false;
    }
    
    return order_engine.handle_market_close_positions(processed_data_for_close);
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
