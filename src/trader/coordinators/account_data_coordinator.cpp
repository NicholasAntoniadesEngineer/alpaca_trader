#include "account_data_coordinator.hpp"
#include "logging/logs/account_logs.hpp"
#include "logging/logs/market_data_logs.hpp"
#include <stdexcept>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::AccountLogs;
using AlpacaTrader::Logging::MarketDataLogs;

AccountDataCoordinator::AccountDataCoordinator(AccountManager& account_manager_ref)
    : account_manager(account_manager_ref) {}

AccountSnapshot AccountDataCoordinator::fetch_current_account_snapshot() {
    return retrieve_account_data_from_manager();
}

void AccountDataCoordinator::update_shared_account_snapshot(AccountDataSnapshotState& snapshot_state) {
    AccountSnapshot current_snapshot = retrieve_account_data_from_manager();
    
    {
        std::lock_guard<std::mutex> state_lock(snapshot_state.state_mutex);
        snapshot_state.account_snapshot = current_snapshot;
        snapshot_state.has_account_flag.store(true);
    }
    
    snapshot_state.data_condition_variable.notify_all();
}

AccountSnapshot AccountDataCoordinator::retrieve_account_data_from_manager() {
    try {
        return account_manager.fetch_account_snapshot();
    } catch (const std::runtime_error& runtime_error) {
        std::string error_message = runtime_error.what();
        if (error_message.find("Failed to fetch account equity") != std::string::npos) {
            AccountLogs::log_account_api_error("Account equity fetch failed: " + error_message, "trading_system.log");
        } else if (error_message.find("Failed to fetch position details") != std::string::npos) {
            AccountLogs::log_position_parse_error("Position details fetch failed: " + error_message, "", "trading_system.log");
        } else if (error_message.find("Failed to fetch open orders count") != std::string::npos) {
            AccountLogs::log_orders_parse_error("Open orders count fetch failed: " + error_message, "", "trading_system.log");
        } else {
            AccountLogs::log_account_api_error("Account snapshot fetch failed: " + error_message, "trading_system.log");
        }
        throw;
    } catch (const std::exception& exception_error) {
        AccountLogs::log_account_api_error("Exception fetching account snapshot: " + std::string(exception_error.what()), "trading_system.log");
        throw;
    } catch (...) {
        AccountLogs::log_account_api_error("Unknown exception fetching account snapshot", "trading_system.log");
        throw std::runtime_error("Unknown exception fetching account snapshot");
    }
}

void AccountDataCoordinator::fetch_and_update_account_data(AccountSnapshot& account_snapshot_ref,
                                                           std::mutex& state_mutex_ref,
                                                           std::condition_variable& data_condition_variable_ref,
                                                           std::atomic<bool>& has_account_flag_ref) {
    AccountDataSnapshotState snapshot_state{
        account_snapshot_ref,
        state_mutex_ref,
        data_condition_variable_ref,
        has_account_flag_ref
    };
    
    update_shared_account_snapshot(snapshot_state);
}

} // namespace Core
} // namespace AlpacaTrader

