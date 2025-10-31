#ifndef MARKET_GATE_LOGS_HPP
#define MARKET_GATE_LOGS_HPP

#include <string>

namespace MarketGateLogs {

void log_thread_starting();
void log_exception(const std::string& error_message);
void log_unknown_exception();
void log_before_initial_hours_check();
void log_initial_hours_state(bool is_within_trading_hours);
void log_before_update_fetch_window();
void log_after_update_fetch_window();
void log_before_connectivity_status();
void log_after_connectivity_status();
void log_loop_exception(const std::string& error_message);
void log_loop_unknown_exception();
void log_loop_exited();
void log_market_gate_loop_exception(const std::string& error_message);
void log_market_gate_loop_unknown_exception();
void log_before_api_check();
void log_current_hours_state(bool is_within_trading_hours);
void log_fetch_gate_state(bool enabled);
void log_connectivity_status_changed(const std::string& status_message);

}

#endif // MARKET_GATE_LOGS_HPP


