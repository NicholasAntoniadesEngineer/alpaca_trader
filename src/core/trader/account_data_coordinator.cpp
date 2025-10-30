#include "account_data_coordinator.hpp"

namespace AlpacaTrader {
namespace Core {

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
    return account_manager.fetch_account_snapshot();
}

} // namespace Core
} // namespace AlpacaTrader

