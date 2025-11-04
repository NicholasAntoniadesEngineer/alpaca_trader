#ifndef MARKET_GATE_LOGS_HPP
#define MARKET_GATE_LOGS_HPP

#include <string>

namespace MarketGateLogs {

void log_fetch_gate_state(bool enabled);
void log_connectivity_status_changed(const std::string& status_message);

}

#endif // MARKET_GATE_LOGS_HPP


