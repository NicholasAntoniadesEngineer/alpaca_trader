#include "market_gate_coordinator.hpp"
#include "logging/logs/market_gate_logs.hpp"

namespace AlpacaTrader {
namespace Core {

MarketGateCoordinator::MarketGateCoordinator(API::ApiManager& api_manager_ref, ConnectivityManager& connectivity_manager_ref)
    : api_manager(api_manager_ref), connectivity_manager(connectivity_manager_ref) {}

void MarketGateCoordinator::check_and_update_fetch_window(const std::string& trading_symbol, std::atomic<bool>& allow_fetch, bool& last_within_trading_hours) {
    bool currently_within_trading_hours = false;
    try {
        currently_within_trading_hours = api_manager.is_within_trading_hours(trading_symbol);
        connectivity_manager.report_success();
    } catch (const std::exception& trading_hours_status_exception_error) {
        connectivity_manager.report_failure(trading_hours_status_exception_error.what());
        currently_within_trading_hours = false;
    } catch (...) {
        connectivity_manager.report_failure("Unknown error in trading hours check");
        currently_within_trading_hours = false;
    }
    if (currently_within_trading_hours != last_within_trading_hours) {
        allow_fetch.store(currently_within_trading_hours);
        last_within_trading_hours = currently_within_trading_hours;
    }
}

void MarketGateCoordinator::check_and_report_connectivity_status(ConnectivityManager::ConnectionStatus& last_connectivity_status) {
    ConnectivityManager::ConnectionStatus current_connectivity_status = connectivity_manager.get_status();
    if (current_connectivity_status != last_connectivity_status) {
        std::string status_message = "Connectivity status changed: " + connectivity_manager.get_status_string();
        auto connectivity_state = connectivity_manager.get_state();
        if (current_connectivity_status == ConnectivityManager::ConnectionStatus::DISCONNECTED) {
            status_message += " (retry in " + std::to_string(connectivity_manager.get_seconds_until_retry()) + "s)";
        } else if (current_connectivity_status == ConnectivityManager::ConnectionStatus::DEGRADED) {
            status_message += " (" + std::to_string(connectivity_state.consecutive_failures) + " failures)";
        }
        MarketGateLogs::log_connectivity_status_changed(status_message);
        last_connectivity_status = current_connectivity_status;
    }
}

ConnectivityManager::ConnectionStatus MarketGateCoordinator::get_connectivity_status() const {
    return connectivity_manager.get_status();
}

} // namespace Core
} // namespace AlpacaTrader

