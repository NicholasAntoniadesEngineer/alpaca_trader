#ifndef MARKET_GATE_COORDINATOR_HPP
#define MARKET_GATE_COORDINATOR_HPP

#include "api/general/api_manager.hpp"
#include "utils/connectivity_manager.hpp"
#include <atomic>
#include <string>

namespace AlpacaTrader {
namespace Core {

class MarketGateCoordinator {
public:
    MarketGateCoordinator(API::ApiManager& api_manager_ref, ConnectivityManager& connectivity_manager_ref);
    
    // Check trading hours and update fetch window
    void check_and_update_fetch_window(const std::string& trading_symbol, std::atomic<bool>& allow_fetch, bool& last_within_trading_hours);
    
    // Check and report connectivity status changes
    void check_and_report_connectivity_status(ConnectivityManager::ConnectionStatus& last_connectivity_status);
    
    // Get current connectivity status
    ConnectivityManager::ConnectionStatus get_connectivity_status() const;

private:
    API::ApiManager& api_manager;
    ConnectivityManager& connectivity_manager;
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MARKET_GATE_COORDINATOR_HPP

