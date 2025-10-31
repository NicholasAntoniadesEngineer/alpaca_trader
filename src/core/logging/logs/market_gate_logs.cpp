#include "market_gate_logs.hpp"
#include "core/logging/logger/async_logger.hpp"

using AlpacaTrader::Logging::log_message;

namespace MarketGateLogs {

void log_thread_starting() { log_message("MarketGateThread starting", "trading_system.log"); }
void log_exception(const std::string& error_message) { log_message("MarketGateThread exception: " + error_message, "trading_system.log"); }
void log_unknown_exception() { log_message("MarketGateThread unknown exception", "trading_system.log"); }
void log_before_initial_hours_check() { log_message("MarketGateThread before initial is_within_trading_hours", "trading_system.log"); }
void log_initial_hours_state(bool is_within_trading_hours) { log_message(std::string("MarketGateThread initial within_trading_hours=") + (is_within_trading_hours?"true":"false"), "trading_system.log"); }
void log_before_update_fetch_window() { log_message("MarketGateThread before check_and_update_fetch_window", "trading_system.log"); }
void log_after_update_fetch_window() { log_message("MarketGateThread after check_and_update_fetch_window", "trading_system.log"); }
void log_before_connectivity_status() { log_message("MarketGateThread before check_and_report_connectivity_status", "trading_system.log"); }
void log_after_connectivity_status() { log_message("MarketGateThread after check_and_report_connectivity_status", "trading_system.log"); }
void log_loop_exception(const std::string& error_message) { log_message("MarketGateThread loop iteration exception: " + error_message, "trading_system.log"); }
void log_loop_unknown_exception() { log_message("MarketGateThread loop iteration unknown exception", "trading_system.log"); }
void log_loop_exited() { log_message("MarketGateThread loop exited", "trading_system.log"); }
void log_market_gate_loop_exception(const std::string& error_message) { log_message("MarketGateThread market_gate_loop exception: " + error_message, "trading_system.log"); }
void log_market_gate_loop_unknown_exception() { log_message("MarketGateThread market_gate_loop unknown exception", "trading_system.log"); }
void log_before_api_check() { log_message("MarketGateThread check_and_update_fetch_window BEFORE api", "trading_system.log"); }
void log_current_hours_state(bool is_within_trading_hours) { log_message(std::string("MarketGateThread check_and_update_fetch_window within_trading_hours=") + (is_within_trading_hours?"true":"false"), "trading_system.log"); }
void log_fetch_gate_state(bool enabled) { log_message(std::string("Market fetch gate ") + (enabled?"ENABLED":"DISABLED") + " (pre/post window applied)", "trading_system.log"); }
void log_connectivity_status_changed(const std::string& status_message) { log_message(status_message, "trading_system.log"); }

}


