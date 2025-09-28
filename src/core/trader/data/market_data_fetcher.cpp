#include "market_data_fetcher.hpp"
#include "core/logging/market_data_logs.hpp"
#include "core/system/system_state.hpp"
#include <cmath>
#include <thread>
#include <chrono>

namespace AlpacaTrader {
namespace Core {

using AlpacaTrader::Logging::MarketDataLogs;

MarketDataFetcher::MarketDataFetcher(API::AlpacaClient& client_ref, AccountManager& account_mgr, const TraderConfig& cfg)
    : client(client_ref), account_manager(account_mgr), config(cfg) {}

ProcessedData MarketDataFetcher::fetch_and_process_data() {
    ProcessedData data;
    MarketDataLogs::log_market_data_fetch_table(config.target.symbol, config.logging.log_file);

    BarRequest br{config.target.symbol, config.strategy.atr_period + config.timing.bar_buffer};
    auto bars = client.get_recent_bars(br);
    if (bars.size() < static_cast<size_t>(config.strategy.atr_period + 2)) {
        if (bars.size() == 0) {
            MarketDataLogs::log_market_data_result_table("No market data available", false, 0, config.logging.log_file);
        } else {
            MarketDataLogs::log_market_data_result_table("Insufficient data for analysis", false, bars.size(), config.logging.log_file);
        }
        MarketDataLogs::log_market_data_result_table("Market data collection failed", false, bars.size(), config.logging.log_file);
        return data;
    }

    MarketDataLogs::log_market_data_attempt_table("Computing indicators", config.logging.log_file);

    data = MarketProcessing::compute_processed_data(bars, config);
    if (data.atr == 0.0) {
        MarketDataLogs::log_market_data_result_table("Indicator computation failed", false, 0, config.logging.log_file);
        return data;
    }

    MarketDataLogs::log_market_data_attempt_table("Getting position and account data", config.logging.log_file);
    SymbolRequest sr{config.target.symbol};
    data.pos_details = account_manager.fetch_position_details(sr);
    data.open_orders = account_manager.fetch_open_orders_count(sr);
    double equity = account_manager.fetch_account_equity();
    data.exposure_pct = (equity > 0.0) ? (std::abs(data.pos_details.current_value) / equity) * 100.0 : 0.0;

    MarketDataLogs::log_current_positions_table(data.pos_details.qty, data.pos_details.current_value, data.pos_details.unrealized_pl, data.exposure_pct, data.open_orders, config.logging.log_file);

    if (data.pos_details.qty != 0 && data.open_orders == 0) {
        MarketDataLogs::log_market_data_result_table("Missing bracket order warning", true, 0, config.logging.log_file);
    }

    return data;
}

std::pair<MarketSnapshot, AccountSnapshot> MarketDataFetcher::fetch_current_snapshots() {
    if (!sync_state_ptr) {
        MarketDataLogs::log_sync_state_error("No sync state available", config.logging.log_file);
        return {MarketSnapshot{}, AccountSnapshot{}};
    }
    
    if (!sync_state_ptr->market || !sync_state_ptr->account) {
        MarketDataLogs::log_sync_state_error("Invalid snapshot pointers", config.logging.log_file);
        return {MarketSnapshot{}, AccountSnapshot{}};
    }
    
    return {*sync_state_ptr->market, *sync_state_ptr->account};
}

void MarketDataFetcher::wait_for_fresh_data(MarketDataSyncState& sync_state) {
    try {
        if (!validate_sync_state_pointers(sync_state)) {
            MarketDataLogs::log_sync_state_error("Invalid sync state pointers", config.logging.log_file);
            return;
        }
        
        std::unique_lock<std::mutex> lock(*sync_state.mtx);
        sync_state.cv->wait_for(lock, std::chrono::seconds(1), [&]{
            return (sync_state.has_market && sync_state.has_market->load()) && 
                   (sync_state.has_account && sync_state.has_account->load());
        });
        
        if (!sync_state.has_market || !sync_state.has_account || 
            !sync_state.has_market->load() || !sync_state.has_account->load()) {
            MarketDataLogs::log_data_timeout(config.logging.log_file);
            return;
        }
        
        MarketDataLogs::log_data_available(config.logging.log_file);
        
        mark_data_as_consumed(sync_state);
        lock.unlock();
        
    } catch (const std::exception& e) {
        MarketDataLogs::log_data_exception("Exception in wait_for_fresh_data: " + std::string(e.what()), config.logging.log_file);
        return;
    } catch (...) {
        MarketDataLogs::log_data_exception("Unknown exception in wait_for_fresh_data", config.logging.log_file);
        return;
    }
}

void MarketDataFetcher::set_sync_state_references(MarketDataSyncState& sync_state) {
    sync_state_ptr = &sync_state;
}

bool MarketDataFetcher::validate_sync_state_pointers(MarketDataSyncState& sync_state) const {
    return sync_state.mtx && sync_state.cv && 
           sync_state.has_market && sync_state.has_account && 
           sync_state.running;
}

void MarketDataFetcher::mark_data_as_consumed(MarketDataSyncState& sync_state) const {
    if (sync_state.has_market) {
        sync_state.has_market->store(false);
    }
}

} // namespace Core
} // namespace AlpacaTrader
