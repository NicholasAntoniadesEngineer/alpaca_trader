#include "market_gate_logs.hpp"
#include "logging/logger/async_logger.hpp"

using AlpacaTrader::Logging::log_message;

namespace MarketGateLogs {

void log_fetch_gate_state(bool enabled) { log_message(std::string("Market fetch gate ") + (enabled?"ENABLED":"DISABLED") + " (pre/post window applied)", "trading_system.log"); }
void log_connectivity_status_changed(const std::string& status_message) { log_message(status_message, "trading_system.log"); }

}


