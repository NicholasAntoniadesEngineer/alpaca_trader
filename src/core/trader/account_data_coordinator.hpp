#ifndef ACCOUNT_DATA_COORDINATOR_HPP
#define ACCOUNT_DATA_COORDINATOR_HPP

#include "configs/system_config.hpp"
#include "data/account_manager.hpp"
#include "data/data_structures.hpp"
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

private:
    AccountManager& account_manager;
    
    AccountSnapshot retrieve_account_data_from_manager();
};

} // namespace Core
} // namespace AlpacaTrader

#endif // ACCOUNT_DATA_COORDINATOR_HPP

