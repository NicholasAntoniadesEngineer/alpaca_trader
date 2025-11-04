#ifndef ACCOUNT_DATA_COORDINATOR_HPP
#define ACCOUNT_DATA_COORDINATOR_HPP

#include "configs/system_config.hpp"
#include "trader/account_management/account_manager.hpp"
#include "trader/data_structures/data_structures.hpp"
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace AlpacaTrader {
namespace Core {

class AccountDataCoordinator {
public:
    struct AccountDataSnapshotState {
        AccountSnapshot& account_snapshot;
        std::mutex& state_mutex;
        std::condition_variable& data_condition_variable;
        std::atomic<bool>& has_account_flag;
    };

    AccountDataCoordinator(AccountManager& account_manager_ref);
    
    AccountSnapshot fetch_current_account_snapshot();
    void update_shared_account_snapshot(AccountDataSnapshotState& snapshot_state);
    void fetch_and_update_account_data(AccountSnapshot& account_snapshot_ref, 
                                       std::mutex& state_mutex_ref,
                                       std::condition_variable& data_condition_variable_ref,
                                       std::atomic<bool>& has_account_flag_ref);

private:
    AccountManager& account_manager;
    
    AccountSnapshot retrieve_account_data_from_manager();
};

} // namespace Core
} // namespace AlpacaTrader

#endif // ACCOUNT_DATA_COORDINATOR_HPP

