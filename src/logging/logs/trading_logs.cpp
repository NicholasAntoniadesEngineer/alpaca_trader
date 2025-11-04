#include "trading_logs.hpp"
#include "startup_logs.hpp"
#include "logging/logger/async_logger.hpp"
#include "logging/logger/logging_macros.hpp"
#include "utils/time_utils.hpp"
#include <iomanip>
#include <sstream>
#include <climits>
#include <string>

namespace AlpacaTrader {
namespace Logging {

using AlpacaTrader::Core::AccountManager;
using AlpacaTrader::Core::ProcessedData;
using AlpacaTrader::Core::SignalDecision;
using AlpacaTrader::Core::FilterResult;

std::string TradingLogs::format_currency(double amount) {
    std::ostringstream oss;
    oss << "$" << std::fixed << std::setprecision(2) << amount;
    return oss.str();
}

std::string TradingLogs::format_percentage(double percentage) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << percentage << "%";
    return oss.str();
}

void TradingLogs::log_startup(const SystemConfig& config, double initial_equity) {
    log_trader_startup_table(
        config,
        initial_equity,
        config.strategy.risk_percentage_per_trade,
        config.strategy.rr_ratio,
        config.timing.thread_market_data_poll_interval_sec
    );
}

void TradingLogs::log_shutdown(unsigned long total_loops, double final_equity) {
    
    log_message("Trading session complete", "");
    log_message("Total loops executed: " + std::to_string(total_loops), "");
    log_message("Final equity: " + format_currency(final_equity), "");
}


void TradingLogs::log_market_status(bool is_open, const std::string& reason) {
    if (is_open) {
        std::string msg = "Market is OPEN";
        if (!reason.empty()) {
            msg += " - " + reason;
        } else {
            msg += " - trading allowed";
        }
        log_message(msg, "");
    } else {
        std::string msg = "Market is CLOSED";
        if (!reason.empty()) {
            msg += " - " + reason;
        }
        
        log_message(msg, "");
    }
}

void TradingLogs::log_trading_conditions(double daily_pnl, double exposure_pct, bool allowed, const SystemConfig& config) {
    LOG_THREAD_TRADING_CONDITIONS_HEADER();
    log_trading_conditions_table(daily_pnl * 100.0, config.strategy.max_daily_loss_percentage * 100.0, config.strategy.daily_profit_target_percentage * 100.0, exposure_pct, config.strategy.max_account_exposure_percentage, allowed);
}

void TradingLogs::log_equity_update(double current_equity) {
    LOG_THREAD_SECTION_HEADER("EQUITY UPDATE");
    std::ostringstream oss;
    oss << "Current Equity: " << format_currency(current_equity) << " (acct poll=5s)";
    LOG_THREAD_CONTENT(oss.str());
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogs::log_market_data_status(bool has_data, size_t data_points) {
    
    if (has_data) {
        log_message("Market data available (" + std::to_string(data_points) + " points)", "");
    } else {
        log_message("No market data available", "");
    }
}

void TradingLogs::log_signal_triggered(const std::string& signal_type, bool triggered) {
    std::ostringstream oss;
    oss << signal_type << " signal " << (triggered ? "TRIGGERED" : "not triggered");
    log_message(oss.str(), "");
}

void TradingLogs::log_filters_passed() {
    log_message("All filters passed - trade allowed", "");
}

void TradingLogs::log_position_closure(const std::string& reason, int quantity) {
    std::ostringstream oss;
    oss << "Position closure: " << reason << " (" << quantity << " shares)";
    log_message(oss.str(), "");
}

void TradingLogs::log_position_limits_reached(const std::string& side) {
    std::ostringstream oss;
    oss << "Position limits reached for " << side << " - trade blocked";
    log_message(oss.str(), "");
}

void TradingLogs::log_no_trading_pattern() {
    log_message("No valid trading pattern detected - no action taken", "");
}

void TradingLogs::log_order_intent(const std::string& side, double entry_price, double stop_loss, double take_profit) {
    
    std::ostringstream oss;
    oss << side << " order intent - Entry: " << format_currency(entry_price)
        << " | SL: " << format_currency(stop_loss)
        << " | TP: " << format_currency(take_profit);
    
    log_message("ORDER", oss.str());
}

void TradingLogs::log_order_result(const std::string& order_id, bool success, const std::string& reason) {
    
    std::ostringstream oss;
    oss << "Order " << order_id << " - " << (success ? "SUCCESS" : "FAILED");
    if (!reason.empty()) {
        oss << " (" << reason << ")";
    }
    
    if (success) {
        log_message("ORDER", oss.str());
    } else {
        log_message("ORDER", oss.str());
    }
}

void TradingLogs::log_comprehensive_order_execution(const ComprehensiveOrderExecutionRequest& order_execution_request) {
    TABLE_HEADER_48("ORDER EXECUTION", "Comprehensive Order Details");
    
    TABLE_ROW_48("Order Type", order_execution_request.order_type);
    TABLE_ROW_48("Side", order_execution_request.side);
    std::ostringstream qty_oss;
    qty_oss << std::fixed << std::setprecision(8) << order_execution_request.quantity;
    TABLE_ROW_48("Quantity", qty_oss.str());
    TABLE_ROW_48("Symbol", order_execution_request.symbol);
    TABLE_ROW_48("Function", order_execution_request.function_name);
    
    TABLE_SEPARATOR_48();
    
    TABLE_ROW_48("Current Price", "$" + std::to_string(order_execution_request.current_price));
    TABLE_ROW_48("ATR", std::to_string(order_execution_request.atr));
    TABLE_ROW_48("Position Qty", std::to_string(order_execution_request.position_quantity));
    TABLE_ROW_48("Risk Amount", "$" + std::to_string(order_execution_request.risk_amount));
    
    if (order_execution_request.stop_loss > 0.0 || order_execution_request.take_profit > 0.0) {
        TABLE_SEPARATOR_48();
        TABLE_ROW_48("Stop Loss", order_execution_request.stop_loss > 0.0 ? "$" + std::to_string(order_execution_request.stop_loss) : "N/A");
        TABLE_ROW_48("Take Profit", order_execution_request.take_profit > 0.0 ? "$" + std::to_string(order_execution_request.take_profit) : "N/A");
    } else if (order_execution_request.order_type == "Market Order") {
        TABLE_SEPARATOR_48();
        TABLE_ROW_48("Order Purpose", "Position Closure");
        TABLE_ROW_48("Entry Price", "$" + std::to_string(order_execution_request.current_price));
        TABLE_ROW_48("Exit Strategy", "Market Price");
    }
    
    TABLE_FOOTER_48();
}

// Consolidated API response logging - shows all API response data in one comprehensive table
void TradingLogs::log_comprehensive_api_response(const std::string& order_id, const std::string& status, 
                                                const std::string& side, const std::string& quantity, 
                                                const std::string& order_class, const std::string& position_intent,
                                                const std::string& created_at, const std::string& filled_at,
                                                const std::string& filled_qty, const std::string& filled_avg_price,
                                                const std::string& error_code, const std::string& error_message,
                                                const std::string& available_qty, const std::string& existing_qty,
                                                const std::string& held_for_orders, const std::string& related_orders) {
    
    // Determine if this is an error response or success response
    bool is_error = !error_code.empty() || !error_message.empty();
    
    if (is_error) {
        TABLE_HEADER_48("API ERROR RESPONSE", "Order Rejection Details");
        
        TABLE_ROW_48("Error Code", error_code.empty() ? "N/A" : error_code);
        TABLE_ROW_48("Error Message", error_message.empty() ? "N/A" : error_message);
        TABLE_ROW_48("Symbol", "N/A");
        TABLE_ROW_48("Requested Qty", quantity.empty() ? "N/A" : quantity);
        TABLE_ROW_48("Available Qty", available_qty.empty() ? "N/A" : available_qty);
        TABLE_ROW_48("Existing Qty", existing_qty.empty() ? "N/A" : existing_qty);
        TABLE_ROW_48("Held for Orders", held_for_orders.empty() ? "N/A" : held_for_orders);
        TABLE_ROW_48("Related Orders", related_orders.empty() ? "N/A" : related_orders);
        
    } else {
        TABLE_HEADER_48("API SUCCESS RESPONSE", "Order Confirmation Details");
        
        TABLE_ROW_48("Order ID", order_id.empty() ? "N/A" : order_id);
        TABLE_ROW_48("Status", status.empty() ? "N/A" : status);
        TABLE_ROW_48("Side", side.empty() ? "N/A" : side);
        TABLE_ROW_48("Quantity", quantity.empty() ? "N/A" : quantity);
        TABLE_ROW_48("Order Class", order_class.empty() ? "N/A" : order_class);
        TABLE_ROW_48("Position Intent", position_intent.empty() ? "N/A" : position_intent);
        TABLE_ROW_48("Created At", created_at.empty() ? "N/A" : created_at);
        TABLE_ROW_48("Filled At", filled_at.empty() ? "Not filled" : filled_at);
        TABLE_ROW_48("Filled Qty", filled_qty.empty() ? "0" : filled_qty);
        TABLE_ROW_48("Filled Avg Price", filled_avg_price.empty() ? "N/A" : "$" + filled_avg_price);
    }
    
    TABLE_FOOTER_48();
}


void TradingLogs::log_market_close_warning(int minutes_until_close) {
    LOG_THREAD_SECTION_HEADER("MARKET CLOSE WARNING");
    std::ostringstream oss;
    oss << "Market closing in " << minutes_until_close << " minutes - preparing to close positions";
    log_message(oss.str(), "");
}

void TradingLogs::log_market_close_position_closure(int quantity, const std::string& symbol, const std::string& side) {
    std::ostringstream oss;
    oss << "Closing position for market close: " << side << " " << std::abs(quantity) << " shares of " << symbol;
    log_message(oss.str(), "");
}

void TradingLogs::log_market_close_complete() {
    log_message("All positions closed for market close - trading halted until next session", "");
    LOG_THREAD_SEPARATOR();
}


// Detailed trading analysis logging

void TradingLogs::log_loop_header(unsigned long loop_number, const std::string& symbol) {
    LOG_TRADING_LOOP_HEADER(loop_number, symbol);
}


void TradingLogs::log_candle_and_signals(const ProcessedData& data, const SignalDecision& signals) {
    log_candle_data_table(data.curr.open_price, data.curr.high_price, data.curr.low_price, data.curr.close_price);
    log_signals_table(signals.buy, signals.sell);
}

void TradingLogs::log_filters(const FilterResult& filters, const SystemConfig& config, const ProcessedData& data) {
    // Use absolute ATR threshold if enabled, otherwise use relative threshold
    double atr_threshold = config.strategy.use_absolute_atr_threshold ? 
                          config.strategy.atr_absolute_minimum_threshold : 
                          config.strategy.entry_signal_atr_multiplier;
    
    // For absolute threshold, pass the actual ATR value instead of ratio
    double atr_value = config.strategy.use_absolute_atr_threshold ? 
                      data.atr : 
                      filters.atr_ratio;
    
    log_filters_table(filters.atr_pass, atr_value, atr_threshold, 
                     filters.vol_pass, filters.vol_ratio, config.strategy.entry_signal_volume_multiplier, 
                     filters.doji_pass);
}

void TradingLogs::log_summary(const ProcessedData& data, const SignalDecision& signals, const FilterResult& filters, const std::string& symbol) {
    if (symbol.empty()) {
        throw std::runtime_error("Symbol is required for log_summary but not provided");
    }
    std::string display_symbol = symbol;
    log_decision_summary_table(display_symbol, data.curr.close_price, signals.buy, signals.sell, 
                              filters.atr_pass, filters.vol_pass, filters.doji_pass, 
                              data.exposure_pct, filters.atr_ratio, filters.vol_ratio);
}

void TradingLogs::log_signal_analysis_start(const std::string& symbol) {
    LOG_THREAD_SIGNAL_ANALYSIS_HEADER(symbol);
    LOG_THREAD_SEPARATOR();
}

void TradingLogs::log_signal_analysis_complete() {
    LOG_THREAD_SEPARATOR();
    LOG_SIGNAL_ANALYSIS_COMPLETE();
    LOG_MESSAGE_BAR();
    
}

void TradingLogs::log_filters_not_met_preview(double risk_amount, int quantity) {
    log_filters_not_met_table(risk_amount, quantity);
}

void TradingLogs::log_filters_not_met_table(double risk_amount, int quantity) {
    TABLE_HEADER_48("Filters Failed", "Trade Skipped - Position Preview");
    
    TABLE_ROW_48("Risk Amount", format_currency(risk_amount) + "/share");
    TABLE_ROW_48("Quantity", std::to_string(quantity) + " shares");
    
    TABLE_SEPARATOR_48();
    
    TABLE_ROW_48("STATUS", "TRADE BLOCKED - Filters not met");
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_position_size(double risk_amount, int quantity) {
    std::ostringstream oss;
    oss << "Position sizing - Risk: " << format_currency(risk_amount) << " | Qty: " << quantity;
    log_message(oss.str(), "");
}

void TradingLogs::log_position_size_with_buying_power(double risk_amount, double quantity, double buying_power, double current_price) {
    LOG_THREAD_POSITION_SIZING_HEADER();
    log_position_sizing_table(risk_amount, quantity, buying_power, current_price);
}

void TradingLogs::log_position_sizing_debug(double risk_based_qty, double exposure_based_qty, double max_value_qty, double buying_power_qty, double final_qty, bool is_crypto_mode) {
    log_sizing_analysis_table(risk_based_qty, exposure_based_qty, max_value_qty, buying_power_qty, final_qty, is_crypto_mode);
}

void TradingLogs::log_current_position(int quantity, const std::string& symbol) {
    LOG_THREAD_CURRENT_POSITION_HEADER();
    std::ostringstream oss;
    if (quantity == 0) {
        oss << "No position in " << symbol;
    } else if (quantity > 0) {
        oss << "LONG " << quantity << " shares of " << symbol;
    } else {
        oss << "SHORT " << (-quantity) << " shares of " << symbol;
    }
    LOG_THREAD_CONTENT(oss.str());
    LOG_THREAD_SEPARATOR();
}

// ========================================================================
// Enhanced Tabulated Logging Functions
// ========================================================================

void TradingLogs::log_position_sizing_table(double risk_amount, double quantity, double buying_power, double current_price) {
    double position_value = quantity * current_price;
    
    TABLE_HEADER_30("Parameter", "Value");
    TABLE_ROW_30("Risk Amount", format_currency(risk_amount));
    std::ostringstream qty_oss;
    qty_oss << std::fixed << std::setprecision(8) << quantity;
    TABLE_ROW_30("Quantity", qty_oss.str());
    TABLE_ROW_30("Position Value", format_currency(position_value));
    TABLE_ROW_30("Buying Power", format_currency(buying_power));
    TABLE_FOOTER_30();
    log_message("", "");
}

void TradingLogs::log_sizing_analysis_table(double risk_based_qty, double exposure_based_qty, double max_value_qty, double buying_power_qty, double final_qty, bool is_crypto_mode) {
    TABLE_HEADER_30("Sizing Analysis", "Calculated Quantities");
    
    std::string unit_label = is_crypto_mode ? "" : " shares";
    std::ostringstream oss;
    
    oss << std::fixed << std::setprecision(8) << risk_based_qty;
    TABLE_ROW_30("Risk-Based", oss.str() + unit_label);
    
    oss.str("");
    oss << std::fixed << std::setprecision(8) << exposure_based_qty;
    TABLE_ROW_30("Exposure-Based", oss.str() + unit_label);
    
    if (max_value_qty < std::numeric_limits<double>::max()) {
        oss.str("");
        oss << std::fixed << std::setprecision(8) << max_value_qty;
        TABLE_ROW_30("Max Value", oss.str() + unit_label);
    }
    
    std::string bp_str;
    if (buying_power_qty >= std::numeric_limits<double>::max() * 0.9) {
        bp_str = "unlimited";
    } else {
        oss.str("");
        oss << std::fixed << std::setprecision(8) << buying_power_qty;
        bp_str = oss.str() + unit_label;
    }
    TABLE_ROW_30("Buying Power", bp_str);
    
    TABLE_SEPARATOR_30();
    
    oss.str("");
    oss << std::fixed << std::setprecision(8) << final_qty;
    TABLE_ROW_30("FINAL QUANTITY", oss.str() + unit_label);
    
    // For crypto, use 0.00001 threshold; for stocks, use 0.001 threshold
    double threshold = is_crypto_mode ? 0.00001 : 0.001;
    if (final_qty < threshold) {
        std::string limitations = "";
        if (risk_based_qty < threshold) limitations += "RISK ";
        if (exposure_based_qty < threshold) limitations += "EXPOSURE ";
        if (max_value_qty < std::numeric_limits<double>::max() && max_value_qty < threshold) limitations += "MAX_VALUE ";
        if (buying_power_qty < threshold) limitations += "BUYING_POWER ";
        if (!limitations.empty()) {
            TABLE_ROW_30("LIMITED BY", limitations);
        }
    }
    
    TABLE_FOOTER_30();
}

void TradingLogs::log_exit_targets_table(const ExitTargetsTableRequest& exit_targets_request) {
    TABLE_HEADER_30("Exit Targets", "Calculated Prices");
    
    TABLE_ROW_30("Order Side", exit_targets_request.side);
    TABLE_ROW_30("Entry Price", format_currency(exit_targets_request.price));
    TABLE_ROW_30("Risk Amount", format_currency(exit_targets_request.risk));
    TABLE_ROW_30("Risk/Reward", "1:" + std::to_string(exit_targets_request.risk_reward_ratio));
    
    TABLE_SEPARATOR_30();
    
    TABLE_ROW_30("Stop Loss", format_currency(exit_targets_request.stop_loss));
    TABLE_ROW_30("Take Profit", format_currency(exit_targets_request.take_profit));
    
    TABLE_FOOTER_30();
}

void TradingLogs::log_order_result_table(const std::string& operation, const std::string& response) {
    TABLE_HEADER_48("Order Result", "Execution Status");
    
    // Parse operation for multiline display
    std::string op_line1 = operation;
    std::string op_line2 = "";
    
    // Check if operation contains bracket order details
    size_t tp_pos = operation.find("(TP:");
    if (tp_pos != std::string::npos) {
        op_line1 = operation.substr(0, tp_pos);
        op_line2 = operation.substr(tp_pos);
        // Remove trailing space from line 1
        if (!op_line1.empty() && op_line1.back() == ' ') {
            op_line1.pop_back();
        }
    }
    
    TABLE_ROW_48("Operation", op_line1);
    
    if (!op_line2.empty()) {
        TABLE_ROW_48("", op_line2);
    }
    
    std::string status = "FAILED";
    std::string order_id = "";
    std::string error_reason = "";
    
    if (!response.empty()) {
        try {
            // Simple JSON parsing to extract order ID and check for errors
            size_t id_pos = response.find("\"id\":");
            if (id_pos != std::string::npos) {
                size_t start = response.find("\"", id_pos + 5) + 1;
                size_t end = response.find("\"", start);
                if (start != std::string::npos && end != std::string::npos) {
                    status = "SUCCESS";
                    order_id = response.substr(start, end - start);
                }
            } else {
                // Check for error message in response
                size_t error_pos = response.find("\"message\":");
                if (error_pos != std::string::npos) {
                    size_t start = response.find("\"", error_pos + 10) + 1;
                    size_t end = response.find("\"", start);
                    if (start != std::string::npos && end != std::string::npos) {
                        error_reason = response.substr(start, end - start);
                        status = "FAILED - " + error_reason;
                    }
                } else {
                    status = "FAILED - Unknown Response";
                }
            }
        } catch (...) {
            status = "FAILED - Parse Error";
        }
    } else {
        status = "FAILED - No Response";
    }
    
    // Show Order ID first if successful
    if (!order_id.empty()) {
        TABLE_ROW_48("Order ID", order_id);
    }
    
    // Add separator before final status for emphasis
    if (!order_id.empty()) {
        TABLE_SEPARATOR_48();
    }
    
    // Final status line - most important information last
    TABLE_ROW_48("RESULT", status);
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_data_source_info_table(const std::string& source, double price, const std::string& status) {
    TABLE_HEADER_48("Data Source", "Market Information");
    TABLE_ROW_48("Feed", source);
    TABLE_ROW_48("Price", format_currency(price));
    TABLE_ROW_48("Status", status);
    TABLE_FOOTER_48();
}

// ========================================================================
// Market Data Fetching Tables
// ========================================================================




void TradingLogs::log_market_data_result_table(const std::string& description, bool success, size_t bar_count) {
    if (success) {
        TABLE_HEADER_48("Market Data", "Connection Result");
        TABLE_ROW_48("Source", description);
        TABLE_ROW_48("RESULT", "SUCCESS - " + std::to_string(bar_count) + " bars");
        TABLE_FOOTER_48();
    }
    // Don't log failures - they're just attempts, final success is what matters
}

// ========================================================================
// System Startup and Status Tables
// ========================================================================

void TradingLogs::log_trader_startup_table(const SystemConfig& config, double initial_equity, double risk_per_trade, double rr_ratio, int loop_interval) {
    TABLE_HEADER_48("Trading Overview", "Current Session");

    TABLE_ROW_48("Trading Symbol", config.strategy.symbol);
    TABLE_ROW_48("Asset Type", config.strategy.is_crypto_asset ? "CRYPTOCURRENCY" : "STOCK");
    TABLE_ROW_48("Initial Equity", format_currency(initial_equity));
    
    std::string risk_display = std::to_string(risk_per_trade * 100.0).substr(0,5) + "%";
    TABLE_ROW_48("Risk per Trade", risk_display);
    
    std::string rr_display = "1:" + std::to_string(rr_ratio).substr(0,6);
    TABLE_ROW_48("Risk/Reward", rr_display);
    
    std::string interval_display = std::to_string(loop_interval) + " seconds";
    TABLE_ROW_48("Loop Interval", interval_display);
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_account_overview_table(const std::string& account_number, const std::string& status, const std::string& currency, bool pattern_day_trader, const std::string& created_date) {
    TABLE_HEADER_48("Account Overview", "Details");
    
    TABLE_ROW_48("Account Number", account_number);
    TABLE_ROW_48("Status", status);
    TABLE_ROW_48("Currency", currency);
    
    std::string pdt_display = pattern_day_trader ? "YES" : "NO";
    TABLE_ROW_48("Pattern Day Trader", pdt_display);
    TABLE_ROW_48("Created", created_date);
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_financial_summary_table(double equity, double last_equity, double cash, double buying_power, double long_market_value, double short_market_value, double initial_margin, double maintenance_margin, double sma, int day_trade_count, double regt_buying_power, double day_trading_buying_power) {
    TABLE_HEADER_48("Financial Summary", "Account Values");
    
    TABLE_ROW_48("Equity", format_currency(equity));
    TABLE_ROW_48("Last Equity", format_currency(last_equity));
    TABLE_ROW_48("Cash", format_currency(cash));
    TABLE_ROW_48("Buying Power", format_currency(buying_power));
    TABLE_ROW_48("Long Market Val", format_currency(long_market_value));
    TABLE_ROW_48("Short Market Val", format_currency(short_market_value));
    TABLE_ROW_48("Initial Margin", format_currency(initial_margin));
    TABLE_ROW_48("Maint Margin", format_currency(maintenance_margin));
    TABLE_ROW_48("SMA", format_currency(sma));
    TABLE_ROW_48("Day Trade Count", std::to_string(day_trade_count));
    TABLE_ROW_48("RegT Buying Power", format_currency(regt_buying_power));
    TABLE_ROW_48("DT Buying Power", format_currency(day_trading_buying_power));
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_current_positions_table(int quantity, double current_value, double unrealized_pnl, double exposure_pct, int open_orders) {
    TABLE_HEADER_48("Current Position", "Portfolio Status");
    
    std::string position_display;
    if (quantity == 0) {
        position_display = "No position";
    } else if (quantity > 0) {
        position_display = "LONG " + std::to_string(quantity) + " shares";
    } else {
        position_display = "SHORT " + std::to_string(-quantity) + " shares";
    }
    TABLE_ROW_48("Position", position_display);
    TABLE_ROW_48("Current Value", format_currency(current_value));
    TABLE_ROW_48("Unrealized P/L", format_currency(unrealized_pnl));
    
    std::string exp_display = std::to_string(exposure_pct).substr(0,4) + "%";
    TABLE_ROW_48("Exposure", exp_display);
    TABLE_ROW_48("Open Orders", std::to_string(open_orders));
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_data_source_table(const std::string& symbol, const std::string& account_type) {
    TABLE_HEADER_48("Data Sources", "Multi-API Configuration");
    
    TABLE_ROW_48("Historical Bars", "Provider-specific feeds");
    TABLE_ROW_48("Real-time Quotes", "Multi-provider routing");
    TABLE_ROW_48("Trading Symbol", symbol);
    TABLE_ROW_48("Account Type", account_type);
    
    TABLE_FOOTER_48();
}


void TradingLogs::log_thread_system_table(bool priorities_enabled, bool cpu_affinity_enabled) {
    TABLE_HEADER_48("Thread System", "Performance Settings");
    
    std::string priorities_display = priorities_enabled ? "ENABLED" : "DISABLED";
    TABLE_ROW_48("Thread Priorities", priorities_display);
    
    std::string affinity_display = cpu_affinity_enabled ? "ENABLED" : "DISABLED";
    TABLE_ROW_48("CPU Affinity", affinity_display);
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_thread_priorities_table(const std::vector<std::tuple<std::string, std::string, bool>>& thread_statuses) {
    TABLE_HEADER_48("Thread Priorities", "Status");
    
    if (thread_statuses.empty()) {
        // Default fallback if no status provided
        TABLE_ROW_48("TRADER", "HIGHEST priority [OK]");
        TABLE_ROW_48("MARKET", "HIGH priority [OK]");
        TABLE_ROW_48("ACCOUNT", "NORMAL priority [OK]");
        TABLE_ROW_48("GATE", "LOW priority [OK]");
        TABLE_ROW_48("LOGGER", "LOWEST priority [OK]");
    } else {
        for (const auto& status : thread_statuses) {
            std::string thread_name = std::get<0>(status);
            std::string priority = std::get<1>(status);
            bool success = std::get<2>(status);
            
            std::string status_display = priority + " priority [" + (success ? "OK" : "FAIL") + "]";
            TABLE_ROW_48(thread_name, status_display);
        }
    }
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_runtime_config_table(const AlpacaTrader::Config::SystemConfig& config) {
    TABLE_HEADER_48("Runtime Config", "System Settings");
    
    // API Configuration
    TABLE_ROW_48("Trading Mode", (config.trading_mode.mode == AlpacaTrader::Config::TradingMode::STOCKS) ? "STOCKS" : "CRYPTO");
    TABLE_ROW_48("Primary Symbol", config.trading_mode.primary_symbol);
    TABLE_ROW_48("API Providers", std::to_string(config.multi_api.providers.size()));
    
    TABLE_SEPARATOR_48();
    
    // Risk Management
    TABLE_ROW_48("Max Exposure", std::to_string((int)config.strategy.max_account_exposure_percentage) + "%");
    TABLE_ROW_48("BP Usage Factor", std::to_string(config.strategy.buying_power_utilization_percentage).substr(0,4));

    std::string daily_loss = (config.strategy.max_daily_loss_percentage == -1) ? "UNLIMITED" : std::to_string(config.strategy.max_daily_loss_percentage) + "%";
    TABLE_ROW_48("Daily Max Loss", daily_loss);
    TABLE_ROW_48("Profit Target", std::to_string(config.strategy.daily_profit_target_percentage) + "%");
    
    TABLE_SEPARATOR_48();
    
    // Timing Configuration
    TABLE_ROW_48("Market Data Poll", std::to_string(config.timing.thread_market_data_poll_interval_sec) + "s");
    TABLE_ROW_48("Account Data Poll", std::to_string(config.timing.thread_account_data_poll_interval_sec) + "s");
    TABLE_ROW_48("Market Gate Poll", std::to_string(config.timing.thread_market_gate_poll_interval_sec) + "s");
    TABLE_ROW_48("Trader Decision Poll", std::to_string(config.timing.thread_trader_poll_interval_sec) + "s");
    TABLE_ROW_48("Logging Poll", std::to_string(config.timing.thread_logging_poll_interval_sec) + "s");
    TABLE_ROW_48("Historical Bars Fetch", std::to_string(config.timing.historical_data_fetch_period_minutes) + "m");
    TABLE_ROW_48("Historical Data Buffer", std::to_string(config.timing.historical_data_buffer_size) + " bars");
    TABLE_ROW_48("Market Status Check", std::to_string(config.timing.thread_market_gate_poll_interval_sec) + "s");
    TABLE_ROW_48("Thread Monitor Log", std::to_string(config.strategy.health_check_interval_sec) + "s");
    
    TABLE_SEPARATOR_48();
    TABLE_ROW_48("Wash Trade Prevention", config.timing.enable_wash_trade_prevention_mechanism ? "Enabled" : "Disabled");
    if (config.timing.enable_wash_trade_prevention_mechanism) {
        TABLE_ROW_48("Min Order Interval", std::to_string(config.timing.minimum_interval_between_orders_seconds) + " seconds");
    }
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_strategy_config_table(const AlpacaTrader::Config::SystemConfig& config) {
    TABLE_HEADER_48("Strategy Config", "Trading Strategy Settings");
    
    // Signal Detection
    std::string buy_equal = config.strategy.buy_signals_allow_equal_close ? "YES" : "NO";
    std::string buy_higher_high = config.strategy.buy_signals_require_higher_high ? "YES" : "NO";
    std::string buy_higher_low = config.strategy.buy_signals_require_higher_low ? "YES" : "NO";
    TABLE_ROW_48("Buy Equal Close", buy_equal);
    TABLE_ROW_48("Buy Higher High", buy_higher_high);
    TABLE_ROW_48("Buy Higher Low", buy_higher_low);
    
    TABLE_SEPARATOR_48();
    
    std::string sell_equal = config.strategy.sell_signals_allow_equal_close ? "YES" : "NO";
    std::string sell_lower_low = config.strategy.sell_signals_require_lower_low ? "YES" : "NO";
    std::string sell_lower_high = config.strategy.sell_signals_require_lower_high ? "YES" : "NO";
    TABLE_ROW_48("Sell Equal Close", sell_equal);
    TABLE_ROW_48("Sell Lower Low", sell_lower_low);
    TABLE_ROW_48("Sell Lower High", sell_lower_high);
    
    TABLE_SEPARATOR_48();
    
    // Filter Thresholds
    TABLE_ROW_48("ATR Multiplier", std::to_string(config.strategy.entry_signal_atr_multiplier).substr(0,4));
    TABLE_ROW_48("Volume Multiplier", std::to_string(config.strategy.entry_signal_volume_multiplier).substr(0,4));
    TABLE_ROW_48("ATR Calc Bars", std::to_string(config.strategy.atr_calculation_bars));
    TABLE_ROW_48("Bars to Fetch", std::to_string(config.strategy.bars_to_fetch_for_calculations));
    TABLE_ROW_48("Minutes/Bar", std::to_string(config.strategy.minutes_per_bar));
    TABLE_ROW_48("Daily Bars TF", config.strategy.daily_bars_timeframe);
    TABLE_ROW_48("Daily Bars Ct", std::to_string(config.strategy.daily_bars_count));
    TABLE_ROW_48("Avg ATR Multi", std::to_string(config.strategy.average_atr_comparison_multiplier).substr(0,4));
    
    TABLE_SEPARATOR_48();
    
    // Risk & Position Management
    std::string risk_pct = std::to_string(config.strategy.risk_percentage_per_trade * 100.0).substr(0,4) + "%";
    TABLE_ROW_48("Risk per Trade", risk_pct);
    TABLE_ROW_48("Max Trade Value", "$" + std::to_string((int)config.strategy.maximum_dollar_value_per_trade));
    TABLE_ROW_48("RR Ratio", "1:" + std::to_string(config.strategy.rr_ratio).substr(0,4));
    
    // Take Profit Configuration
    if (config.strategy.use_take_profit_percentage) {
        std::string tp_pct = std::to_string(config.strategy.take_profit_percentage * 100.0).substr(0,4) + "%";
        TABLE_ROW_48("Take Profit Method", "Percentage (" + tp_pct + ")");
    } else {
        TABLE_ROW_48("Take Profit Method", "Risk/Reward Ratio");
    }
    
    // Position Scaling Configuration
    if (config.strategy.enable_fixed_share_quantity_per_trade) {
        TABLE_ROW_48("Fixed Shares", "Enabled (" + std::to_string(config.strategy.fixed_share_quantity_per_trade) + " shares)");
    } else {
        TABLE_ROW_48("Fixed Shares", "Disabled");
    }
    
    if (config.strategy.enable_risk_based_position_multiplier) {
        std::string multiplier_str = (config.strategy.risk_based_position_size_multiplier == 1.0) ? 
            "1.0x (Normal)" : std::to_string(config.strategy.risk_based_position_size_multiplier).substr(0,4) + "x";
        TABLE_ROW_48("Position Multiplier", "Enabled (" + multiplier_str + ")");
    } else {
        TABLE_ROW_48("Position Multiplier", "Disabled");
    }
    
    std::string multi_pos = config.strategy.allow_multiple_positions_per_symbol ? "YES" : "NO";
    TABLE_ROW_48("Multi Positions", multi_pos);
    TABLE_ROW_48("Max Layers", std::to_string(config.strategy.maximum_position_layers));

    std::string close_reverse = config.strategy.close_positions_on_signal_reversal ? "YES" : "NO";
    TABLE_ROW_48("Close on Reverse", close_reverse);
    
    TABLE_FOOTER_48();
}

// ========================================================================
// Trading Decision Tables
// ========================================================================

void TradingLogs::log_trading_conditions_table(double daily_pnl_pct, double daily_loss_limit, double daily_profit_target, double exposure_pct, double max_exposure_pct, bool conditions_met) {
    TABLE_HEADER_48("Trading Conditions", "Current Values");
    
    std::ostringstream pnl_oss;
    pnl_oss << std::fixed << std::setprecision(3) << daily_pnl_pct << "%";
    std::string pnl_limits = "(" + std::to_string(daily_loss_limit).substr(0,6) + "% to " + std::to_string(daily_profit_target).substr(0,5) + "%)";
    std::string pnl_display = pnl_oss.str() + " " + pnl_limits;
    
    TABLE_ROW_48("Daily P/L", pnl_display);
    
    std::string exp_display = std::to_string((int)exposure_pct) + "% (Max: " + std::to_string((int)max_exposure_pct) + "%)";
    TABLE_ROW_48("Exposure", exp_display);
    
    TABLE_SEPARATOR_48();
    
    std::string result = conditions_met ? "All conditions met - Trading allowed" : "Conditions not met - Trading blocked";
    TABLE_ROW_48("RESULT", result);
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_candle_data_table(double open, double high, double low, double close) {
    TABLE_HEADER_48("Candle Data", "OHLC Values");
    TABLE_ROW_48("Open", format_currency(open));
    TABLE_ROW_48("High", format_currency(high));
    TABLE_ROW_48("Low", format_currency(low));
    TABLE_ROW_48("Close", format_currency(close));
    TABLE_FOOTER_48();
}

void TradingLogs::log_signals_table(bool buy_signal, bool sell_signal) {
    TABLE_HEADER_48("Signal Analysis", "Detection Results");
    TABLE_ROW_48("BUY Signal", (buy_signal ? "YES" : "NO"));
    TABLE_ROW_48("SELL Signal", (sell_signal ? "YES" : "NO"));
    TABLE_FOOTER_48();
}

void TradingLogs::log_signals_table_enhanced(const SignalDecision& signals) {
    TABLE_HEADER_48("Signal Analysis", "Detection Results");
    
    std::string buy_status = signals.buy ? "YES" : "NO";
    if (signals.buy) {
        buy_status += " (Strength: " + std::to_string(signals.signal_strength).substr(0,4) + ")";
    }
    TABLE_ROW_48("BUY Signal", buy_status);
    
    std::string sell_status = signals.sell ? "YES" : "NO";
    if (signals.sell) {
        sell_status += " (Strength: " + std::to_string(signals.signal_strength).substr(0,4) + ")";
    }
    TABLE_ROW_48("SELL Signal", sell_status);
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_filters_table(bool atr_pass, double atr_value, double atr_threshold, bool volume_pass, double volume_ratio, double volume_threshold, bool doji_pass) {
    TABLE_HEADER_48("Filter Analysis", "Validation Results");
    
    std::string atr_status = atr_pass ? "PASS" : "FAIL";
    std::string atr_detail;
    
    // Check if this is an absolute threshold (less than 10) or relative threshold (greater than 1)
    if (atr_threshold < 10.0) {
        // Absolute threshold - show actual ATR value vs threshold
        atr_detail = "($" + std::to_string(atr_value).substr(0,4) + " > $" + std::to_string(atr_threshold).substr(0,4) + ")";
    } else {
        // Relative threshold - show ratio
        atr_detail = "(" + std::to_string(atr_value).substr(0,4) + "x > " + std::to_string(atr_threshold).substr(0,4) + "x)";
    }
    
    std::string atr_display = atr_status + " " + atr_detail;
    TABLE_ROW_48("ATR Filter", atr_display);
    
    std::string vol_status = volume_pass ? "PASS" : "FAIL";
    std::string vol_detail = "(" + std::to_string(volume_ratio).substr(0,4) + "x > " + std::to_string(volume_threshold).substr(0,4) + "x)";
    std::string vol_display = vol_status + " " + vol_detail;
    TABLE_ROW_48("Volume Filter", vol_display);
    
    std::string doji_status = doji_pass ? "PASS" : "FAIL";
    TABLE_ROW_48("Doji Filter", doji_status);
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_decision_summary_table(const std::string& symbol, double price, bool buy_signal, bool sell_signal, bool atr_pass, bool volume_pass, bool doji_pass, double exposure_pct, double atr_ratio, double volume_ratio) {
    TABLE_HEADER_48("Decision Summary", "Trading Analysis Results");
    
    std::string price_display = symbol + " @ " + format_currency(price);
    TABLE_ROW_48("Symbol & Price", price_display);
    
    std::string signals_display = "BUY=" + std::string(buy_signal ? "YES" : "NO") + "  SELL=" + std::string(sell_signal ? "YES" : "NO");
    TABLE_ROW_48("Signals", signals_display);
    
    std::string filters_display = "ATR=" + std::string(atr_pass ? "PASS" : "FAIL") + " VOL=" + std::string(volume_pass ? "PASS" : "FAIL") + " DOJI=" + std::string(doji_pass ? "PASS" : "FAIL");
    TABLE_ROW_48("Filters", filters_display);
    
    std::string exp_display = std::to_string((int)exposure_pct) + "%";
    TABLE_ROW_48("Exposure", exp_display);
    
    std::string ratios_display = "ATR=" + std::to_string(atr_ratio).substr(0,5) + "x  VOL=" + std::to_string(volume_ratio).substr(0,5) + "x";
    TABLE_ROW_48("Ratios", ratios_display);
    
    TABLE_FOOTER_48();
}

// Order cancellation logging methods
void TradingLogs::log_cancellation_start(const std::string& strategy, const std::string& signal_side) {
    TABLE_HEADER_48("ORDER CANCELLATION", strategy + " strategy");
    if (!signal_side.empty()) {
        TABLE_ROW_48("Signal", signal_side);
    }
    TABLE_FOOTER_48();
}

void TradingLogs::log_orders_found(int count, const std::string& symbol) {
    TABLE_HEADER_48("ORDERS FOUND", symbol);
    TABLE_ROW_48("Count", std::to_string(count));
    TABLE_FOOTER_48();
}

void TradingLogs::log_orders_filtered(int count, const std::string& reason) {
    TABLE_HEADER_48("ORDERS FILTERED", reason);
    TABLE_ROW_48("Selected", std::to_string(count));
    TABLE_FOOTER_48();
}

void TradingLogs::log_cancellation_complete(int cancelled_count, const std::string& symbol) {
    TABLE_HEADER_48("CANCELLATION COMPLETE", symbol);
    TABLE_ROW_48("Cancelled", std::to_string(cancelled_count));
    TABLE_FOOTER_48();
}

void TradingLogs::log_no_orders_to_cancel() {
    TABLE_HEADER_48("NO ORDERS TO CANCEL", "Current strategy");
    TABLE_ROW_48("Status", "No orders found");
    TABLE_FOOTER_48();
}

// Position management logging methods
void TradingLogs::log_position_closure_start(int quantity) {
    TABLE_HEADER_48("POSITION CLOSURE", "Starting process");
    TABLE_ROW_48("Quantity", std::to_string(quantity));
    TABLE_FOOTER_48();
}

void TradingLogs::log_fresh_position_data(int quantity) {
    TABLE_HEADER_48("FRESH POSITION DATA", "Current quantity");
    TABLE_ROW_48("Quantity", std::to_string(quantity));
    TABLE_FOOTER_48();
}

void TradingLogs::log_position_already_closed() {
    TABLE_HEADER_48("POSITION ALREADY CLOSED", "No action needed");
    TABLE_ROW_48("Status", "Position closed");
    TABLE_FOOTER_48();
}

void TradingLogs::log_closure_order_submitted(const std::string& side, int quantity) {
    TABLE_HEADER_48("CLOSURE ORDER SUBMITTED", side + " order");
    TABLE_ROW_48("Quantity", std::to_string(quantity));
    TABLE_ROW_48("Side", side);
    TABLE_FOOTER_48();
}

void TradingLogs::log_position_verification(int final_quantity) {
    if (final_quantity == 0) {
        TABLE_HEADER_48("POSITION VERIFICATION", "Success");
        TABLE_ROW_48("Status", "Position closed");
        TABLE_FOOTER_48();
    } else {
        TABLE_HEADER_48("POSITION VERIFICATION", "WARNING");
        TABLE_ROW_48("Status", "Position still exists");
        TABLE_ROW_48("Quantity", std::to_string(final_quantity));
        TABLE_FOOTER_48();
    }
}

// Debug and validation logging
void TradingLogs::log_trade_validation_failed(const std::string& reason) {
    log_message("Trade validation failed - " + reason, "");
}

void TradingLogs::log_insufficient_buying_power(double required_buying_power, double available_buying_power, int quantity, double current_price) {
    std::ostringstream oss;
    oss << "Insufficient buying power: Need $" << std::fixed << std::setprecision(2) << required_buying_power 
        << ", Have $" << std::fixed << std::setprecision(2) << available_buying_power 
        << " (Position: " << quantity << " @ $" << std::fixed << std::setprecision(2) << current_price << ")";
    log_message(oss.str(), "");
}

void TradingLogs::log_position_sizing_skipped(const std::string& reason) {
    log_message("Position sizing resulted in " + reason + ", skipping trade", "");
}

void TradingLogs::log_debug_position_data(int current_position_quantity, double position_value, int position_quantity_value, bool is_long, bool is_short) {
    LOG_THREAD_SECTION_HEADER("POSITION DEBUG");
    LOG_THREAD_CONTENT("Current Quantity: " + std::to_string(current_position_quantity));
    LOG_THREAD_CONTENT("Position Value: $" + std::to_string(position_value));
    LOG_THREAD_CONTENT("Position Qty: " + std::to_string(position_quantity_value));
    LOG_THREAD_CONTENT("Is Long: " + std::string(is_long ? "true" : "false") + ", Is Short: " + std::string(is_short ? "true" : "false"));
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogs::log_realtime_price_used(double realtime_price, double delayed_price) {
    LOG_THREAD_SECTION_HEADER("REAL-TIME PRICE VERIFICATION");
    LOG_THREAD_CONTENT("Using real-time price: $" + std::to_string(realtime_price));
    LOG_THREAD_CONTENT("Delayed price: $" + std::to_string(delayed_price));
    LOG_THREAD_CONTENT("Price difference: $" + std::to_string(realtime_price - delayed_price));
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogs::log_realtime_price_fallback(double delayed_price) {
    LOG_THREAD_SECTION_HEADER("REAL-TIME PRICE VERIFICATION");
    LOG_THREAD_CONTENT("Real-time price unavailable");
    LOG_THREAD_CONTENT("Using delayed price: $" + std::to_string(delayed_price));
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogs::log_debug_account_details(int position_quantity, double current_value) {
    LOG_THREAD_SECTION_HEADER("ACCOUNT DEBUG");
    LOG_THREAD_CONTENT("Fresh Quantity: " + std::to_string(position_quantity));
    LOG_THREAD_CONTENT("Current Value: $" + std::to_string(current_value));
    LOG_THREAD_SECTION_FOOTER();
}


void TradingLogs::log_debug_fresh_data_fetch(const std::string& position_type) {
    LOG_THREAD_SECTION_HEADER("FRESH DATA FETCH");
    LOG_THREAD_CONTENT("Forcing fresh account data fetch before closing " + position_type + " position");
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogs::log_debug_fresh_position_data(int fresh_position_quantity, int current_position_quantity) {
    LOG_THREAD_SECTION_HEADER("POSITION DATA UPDATE");
    LOG_THREAD_CONTENT("Fresh Quantity: " + std::to_string(fresh_position_quantity));
    LOG_THREAD_CONTENT("Previous Quantity: " + std::to_string(current_position_quantity));
    LOG_THREAD_SECTION_FOOTER();
}


void TradingLogs::log_debug_position_closure_attempt(int position_quantity) {
    LOG_THREAD_SECTION_HEADER("POSITION CLOSURE ATTEMPT");
    LOG_THREAD_CONTENT("Attempting to close fresh position: " + std::to_string(position_quantity));
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogs::log_debug_position_closure_attempted() {
    LOG_THREAD_SECTION_HEADER("POSITION CLOSURE STATUS");
    LOG_THREAD_CONTENT("Position closure attempted, waiting for settlement");
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogs::log_debug_position_verification(int verify_position_quantity) {
    LOG_THREAD_SECTION_HEADER("POSITION VERIFICATION");
    LOG_THREAD_CONTENT("Verifying position quantity: " + std::to_string(verify_position_quantity));
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogs::log_debug_position_still_exists(const std::string& side) {
    LOG_THREAD_SECTION_HEADER("POSITION CLOSURE FAILED");
    LOG_THREAD_CONTENT("Position still exists after closure attempt, skipping " + side + " order");
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogs::log_debug_no_position_found(const std::string& side) {
    LOG_THREAD_SECTION_HEADER("POSITION VERIFICATION");
    LOG_THREAD_CONTENT("No " + side + " position found in fresh data, proceeding with " + side);
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogs::log_debug_skipping_trading_cycle() {
    LOG_THREAD_SECTION_HEADER("TRADING CYCLE SKIPPED");
    LOG_THREAD_CONTENT("No fresh market data available");
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogs::log_market_order_intent(const std::string& side, int quantity) {
    LOG_THREAD_SECTION_HEADER("MARKET ORDER INTENT");
    LOG_THREAD_CONTENT("Side: " + side);
    LOG_THREAD_CONTENT("Quantity: " + std::to_string(quantity));
    LOG_THREAD_SECTION_FOOTER();
}

// Inline status and countdown logging
void TradingLogs::log_inline_halt_status(int seconds) {
    log_inline_status(get_formatted_inline_message("|   TRADING HALTED - Next check in " + std::to_string(seconds) + "s"));
}

void TradingLogs::log_inline_next_loop(int seconds) {
    log_inline_status(get_formatted_inline_message("   ⏳ Next loop in " + std::to_string(seconds) + "s   "));
}

void TradingLogs::end_inline_status() {
    AlpacaTrader::Logging::end_inline_status();
}

// Order execution header
void TradingLogs::log_order_execution_header() {
    LOG_THREAD_ORDER_EXECUTION_HEADER();
}

// Order submission logging
void TradingLogs::log_order_submission(const std::string& order_type, const std::string& symbol, const std::string& side, 
                                      double quantity, double price, const std::string& time_in_force, bool is_crypto_mode) {
    TABLE_HEADER_48("ORDER SUBMISSION", "Submitting Order to Alpaca API");
    
    TABLE_ROW_48("Order Type", order_type);
    TABLE_ROW_48("Symbol", symbol);
    TABLE_ROW_48("Side", side);
    std::ostringstream qty_stream;
    qty_stream << std::fixed << std::setprecision(8) << quantity;
    TABLE_ROW_48("Quantity", qty_stream.str());
    std::ostringstream price_stream;
    price_stream << "$" << std::fixed << std::setprecision(2) << price;
    TABLE_ROW_48("Price", price_stream.str());
    TABLE_ROW_48("Time in Force", time_in_force);
    TABLE_ROW_48("Crypto Mode", is_crypto_mode ? "YES" : "NO");
    
    TABLE_FOOTER_48();
}

// Order accepted confirmation logging
void TradingLogs::log_order_accepted(const std::string& order_type, const std::string& symbol, const std::string& side,
                                     double quantity, const std::string& order_id, const std::string& status,
                                     const std::string& filled_qty, const std::string& filled_avg_price,
                                     const std::string& submitted_at, double stop_price, double limit_price) {
    TABLE_HEADER_48("ORDER ACCEPTED", "Alpaca API Confirmation");
    
    std::ostringstream qty_stream;
    qty_stream << std::fixed << std::setprecision(8) << quantity;
    TABLE_ROW_48("Order Type", order_type);
    TABLE_ROW_48("Symbol", symbol);
    TABLE_ROW_48("Side", side);
    TABLE_ROW_48("Quantity", qty_stream.str());
    TABLE_ROW_48("Order ID", order_id.empty() ? "N/A" : order_id);
    TABLE_ROW_48("Status", status.empty() ? "N/A" : status);
    
    if (!filled_qty.empty() && filled_qty != "0") {
        TABLE_SEPARATOR_48();
        TABLE_ROW_48("Filled Quantity", filled_qty);
        if (!filled_avg_price.empty()) {
            std::ostringstream filled_price_stream;
            filled_price_stream << "$" << filled_avg_price;
            TABLE_ROW_48("Filled Avg Price", filled_price_stream.str());
        }
    }
    
    if (!submitted_at.empty()) {
        TABLE_SEPARATOR_48();
        TABLE_ROW_48("Submitted At", submitted_at);
    }
    
    if (stop_price > 0.0 || limit_price > 0.0) {
        TABLE_SEPARATOR_48();
        if (stop_price > 0.0) {
            std::ostringstream stop_stream;
            stop_stream << "$" << std::fixed << std::setprecision(2) << stop_price;
            TABLE_ROW_48("Stop Price", stop_stream.str());
        }
        
        if (limit_price > 0.0) {
            std::ostringstream limit_stream;
            limit_stream << "$" << std::fixed << std::setprecision(2) << limit_price;
            TABLE_ROW_48("Limit Price", limit_stream.str());
        }
    }
    
    TABLE_FOOTER_48();
}

// Full API response logging (condensed to key fields)
void TradingLogs::log_api_response_full(const std::string& api_response_json) {
    // Log full JSON to file (not in table format for readability)
    log_message("Full Alpaca API Response JSON: " + api_response_json, "");
}

// Crypto bracket simulation logging
void TradingLogs::log_crypto_bracket_simulation(const std::string& symbol, const std::string& side, double quantity,
                                               double entry_price, double stop_loss, double take_profit) {
    TABLE_HEADER_48("CRYPTO BRACKET", "Simulating Bracket Order");
    
    TABLE_ROW_48("Symbol", symbol);
    TABLE_ROW_48("Side", side);
    std::ostringstream qty_stream;
    qty_stream << std::fixed << std::setprecision(8) << quantity;
    TABLE_ROW_48("Quantity", qty_stream.str());
    
    TABLE_SEPARATOR_48();
    
    std::ostringstream entry_stream;
    entry_stream << "$" << std::fixed << std::setprecision(2) << entry_price;
    TABLE_ROW_48("Entry Price", entry_stream.str());
    
    std::ostringstream stop_stream;
    stop_stream << "$" << std::fixed << std::setprecision(2) << stop_loss;
    TABLE_ROW_48("Stop Loss", stop_stream.str());
    
    std::ostringstream take_profit_stream;
    take_profit_stream << "$" << std::fixed << std::setprecision(2) << take_profit;
    TABLE_ROW_48("Take Profit", take_profit_stream.str());
    
    TABLE_SEPARATOR_48();
    
    TABLE_ROW_48("Strategy", "Placing entry, then separate stop-loss and take-profit orders");
    
    TABLE_FOOTER_48();
}

// Crypto bracket simulation complete
void TradingLogs::log_crypto_bracket_complete() {
    TABLE_HEADER_48("BRACKET COMPLETE", "All Orders Placed Successfully");
    
    TABLE_ROW_48("Entry Order", "Placed");
    TABLE_ROW_48("Stop-Loss Order", "Placed");
    TABLE_ROW_48("Take-Profit Order", "Placed");
    TABLE_ROW_48("Status", "Complete");
    
    TABLE_FOOTER_48();
}

// Enhanced signal analysis logging
void TradingLogs::log_signal_analysis_detailed(const ProcessedData& data, const SignalDecision& signals, const SystemConfig& config) {
    LOG_THREAD_SECTION_HEADER("DETAILED SIGNAL ANALYSIS");
    
    // Log momentum analysis
    log_momentum_analysis(data, config);
    
    // Log signal strength breakdown
    log_signal_strength_breakdown(signals, config);
    
    LOG_THREAD_SECTION_FOOTER();
}

void TradingLogs::log_momentum_analysis(const ProcessedData& data, const SystemConfig& config) {
    // Calculate momentum indicators
    double price_change = data.curr.close_price - data.prev.close_price;
    double price_change_pct = (data.prev.close_price > 0.0) ? (price_change / data.prev.close_price) * 100.0 : 0.0;
    
    double volume_change = static_cast<double>(data.curr.volume) - static_cast<double>(data.prev.volume);
    double volume_change_pct = (data.prev.volume > 0) ? (volume_change / static_cast<double>(data.prev.volume)) * 100.0 : 0.0;
    
    double volatility_pct = (data.prev.close_price > 0.0) ? (data.atr / data.prev.close_price) * 100.0 : 0.0;
    
    // Log momentum analysis table
    TABLE_HEADER_48("Momentum Analysis", "Current vs Previous Values");
    
    // Show actual price values for debugging
    std::string price_debug = "Prev: $" + std::to_string(data.prev.close_price).substr(0,6) + " | Curr: $" + std::to_string(data.curr.close_price).substr(0,6);
    TABLE_ROW_48("Price Values", price_debug);
    
    std::string price_status = (price_change_pct > config.strategy.minimum_price_change_percentage_for_momentum) ? "PASS" : "FAIL";
    std::string price_detail = "($" + std::to_string(price_change_pct).substr(0,6) + "% > " + std::to_string(config.strategy.minimum_price_change_percentage_for_momentum).substr(0,6) + "%)";
    TABLE_ROW_48("Price Change", price_status + " " + price_detail);
    
    std::string volume_status = (volume_change_pct > config.strategy.minimum_volume_increase_percentage_for_buy_signals) ? "PASS" : "FAIL";
    std::string volume_detail = "(" + std::to_string(volume_change_pct).substr(0,4) + "% > " + std::to_string(config.strategy.minimum_volume_increase_percentage_for_buy_signals).substr(0,4) + "%)";
    TABLE_ROW_48("Volume Change", volume_status + " " + volume_detail);
    
    std::string volatility_status = (volatility_pct > config.strategy.minimum_volatility_percentage_for_buy_signals) ? "PASS" : "FAIL";
    std::string volatility_detail = "(" + std::to_string(volatility_pct).substr(0,4) + "% > " + std::to_string(config.strategy.minimum_volatility_percentage_for_buy_signals).substr(0,4) + "%)";
    TABLE_ROW_48("Volatility", volatility_status + " " + volatility_detail);
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_signal_strength_breakdown(const SignalDecision& signals, const SystemConfig& config) {
    TABLE_HEADER_48("Signal Strength Analysis", "Decision Breakdown");
    
    std::string signal_status = signals.buy ? "BUY" : (signals.sell ? "SELL" : "NONE");
    std::string strength_detail = "(" + std::to_string(signals.signal_strength).substr(0,4) + " >= " + std::to_string(config.strategy.minimum_signal_strength_threshold).substr(0,4) + ")";
    TABLE_ROW_48("Signal Type", signal_status + " " + strength_detail);
    
    TABLE_ROW_48("Signal Strength", std::to_string(signals.signal_strength).substr(0,4) + "/1.0");
    TABLE_ROW_48("Threshold", std::to_string(config.strategy.minimum_signal_strength_threshold).substr(0,4) + "/1.0");
    TABLE_ROW_48("Reason", signals.signal_reason.empty() ? "No analysis" : signals.signal_reason);
    
    TABLE_FOOTER_48();
}

void TradingLogs::log_position_sizing_csv(const AlpacaTrader::Core::PositionSizing& position_sizing_result, const AlpacaTrader::Core::ProcessedData& processed_data_input, const SystemConfig& system_config, double available_buying_power) {
    try {
        std::string timestamp = TimeUtils::get_current_human_readable_time();
        if (system_config.trading_mode.primary_symbol.empty()) {
            throw std::runtime_error("Primary symbol is required but not configured");
        }
        std::string symbol = system_config.trading_mode.primary_symbol;

        if (auto csv = AlpacaTrader::Logging::get_logging_context()->csv_trade_logger) {
            csv->log_position_sizing(
                timestamp, symbol, position_sizing_result.quantity, position_sizing_result.risk_amount,
                position_sizing_result.quantity * processed_data_input.curr.close_price, available_buying_power
            );
        }
    } catch (const std::exception& exception_error) {
        TradingLogs::log_market_data_result_table("CSV logging error in position sizing: " + std::string(exception_error.what()), false, 0);
    } catch (...) {
        TradingLogs::log_market_data_result_table("Unknown CSV logging error in position sizing", false, 0);
    }
}

} // namespace Logging
} // namespace AlpacaTrader

