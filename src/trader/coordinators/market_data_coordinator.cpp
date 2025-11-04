#include "market_data_coordinator.hpp"
#include "trader/market_data/market_bars_manager.hpp"
#include "trader/market_data/market_data_validator.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "configs/system_config.hpp"
#include "logging/logs/market_data_thread_logs.hpp"
#include "logging/logs/market_data_logs.hpp"
#include "logging/logger/logging_macros.hpp"
#include "logging/logger/async_logger.hpp"
#include "utils/time_utils.hpp"
#include "api/polygon/polygon_crypto_client.hpp"
#include <chrono>

using namespace AlpacaTrader::Logging;

namespace AlpacaTrader {
namespace Core {

MarketDataCoordinator::MarketDataCoordinator(MarketDataManager& market_data_manager_ref)
    : market_data_manager(market_data_manager_ref) {}

ProcessedData MarketDataCoordinator::fetch_and_process_market_data(const std::string& trading_symbol, std::vector<Bar>& historical_bars_output) {
    // Validate symbol matches MarketDataManager's configured symbol
    const SystemConfig& manager_config = market_data_manager.get_config();
    if (!trading_symbol.empty() && trading_symbol != manager_config.strategy.symbol) {
        MarketDataThreadLogs::log_thread_loop_exception("Symbol mismatch: requested " + trading_symbol + " but manager configured for " + manager_config.strategy.symbol);
    }
    
    try {
        // Log market data fetch start
        MarketDataLogs::log_market_data_fetch_table(trading_symbol, manager_config.logging.log_file);
        
        // MarketDataManager fetches bars internally and returns them to avoid duplicate fetching
        auto fetch_result = market_data_manager.fetch_and_process_market_data();
        
        ProcessedData processed_data = fetch_result.first;
        historical_bars_output = fetch_result.second;
        
        // Log position data and warnings
        MarketDataLogs::log_position_data_and_warnings(
            processed_data.pos_details.position_quantity,
            processed_data.pos_details.current_value,
            processed_data.pos_details.unrealized_pl,
            processed_data.exposure_pct,
            processed_data.open_orders,
            manager_config.logging.log_file,
            manager_config.strategy.position_long_string,
            manager_config.strategy.position_short_string
        );
        
        return processed_data;
        
    } catch (const std::runtime_error& runtime_error) {
        // Try to extract bars count from error message if available
        size_t bars_count = 0;
        std::string error_msg = runtime_error.what();
        size_t bars_pos = error_msg.find("Bars fetched: ");
        if (bars_pos != std::string::npos) {
            size_t num_start = bars_pos + 14;
            size_t num_end = error_msg.find(".", num_start);
            if (num_end != std::string::npos) {
                try {
                    bars_count = std::stoul(error_msg.substr(num_start, num_end - num_start));
                } catch (...) {
                    // Ignore parse errors
                }
            }
        }
        
        bool isWebSocketActiveFlagValue = false;
        try {
            API::ApiManager& apiManagerReference = market_data_manager.get_api_manager();
            if (apiManagerReference.is_crypto_symbol(trading_symbol)) {
                API::PolygonCryptoClient* polygonClientPointer = apiManagerReference.get_polygon_crypto_client();
                if (polygonClientPointer) {
                    isWebSocketActiveFlagValue = polygonClientPointer->is_websocket_active();
                }
            }
        } catch (...) {
            // Could not determine WebSocket status, default to false
        }
        
        MarketDataLogs::log_market_data_failure_table(
            trading_symbol,
            "Runtime Error",
            error_msg,
            bars_count,
            isWebSocketActiveFlagValue,
            manager_config.logging.log_file
        );
        return ProcessedData{};
    } catch (const std::exception& exception_error) {
        bool isWebSocketActiveFlagValue = false;
        try {
            API::ApiManager& apiManagerReference = market_data_manager.get_api_manager();
            if (apiManagerReference.is_crypto_symbol(trading_symbol)) {
                API::PolygonCryptoClient* polygonClientPointer = apiManagerReference.get_polygon_crypto_client();
                if (polygonClientPointer) {
                    isWebSocketActiveFlagValue = polygonClientPointer->is_websocket_active();
                }
            }
        } catch (...) {
            // Could not determine WebSocket status, default to false
        }
        
        MarketDataLogs::log_market_data_failure_table(
            trading_symbol,
            "Exception",
            std::string("Exception in fetch_and_process_market_data: ") + exception_error.what(),
            0,
            isWebSocketActiveFlagValue,
            manager_config.logging.log_file
        );
        return ProcessedData{};
    } catch (...) {
        bool isWebSocketActiveFlagValue = false;
        try {
            API::ApiManager& apiManagerReference = market_data_manager.get_api_manager();
            if (apiManagerReference.is_crypto_symbol(trading_symbol)) {
                API::PolygonCryptoClient* polygonClientPointer = apiManagerReference.get_polygon_crypto_client();
                if (polygonClientPointer) {
                    isWebSocketActiveFlagValue = polygonClientPointer->is_websocket_active();
                }
            }
        } catch (...) {
            // Could not determine WebSocket status, default to false
        }
        
        MarketDataLogs::log_market_data_failure_table(
            trading_symbol,
            "Unknown Error",
            "Unknown exception in fetch_and_process_market_data",
            0,
            isWebSocketActiveFlagValue,
            manager_config.logging.log_file
        );
        return ProcessedData{};
    }
}

void MarketDataCoordinator::update_shared_market_snapshot(const ProcessedData& processed_data_result, MarketDataSnapshotState& snapshot_state) {
    if (processed_data_result.atr == 0.0) {
        MarketDataThreadLogs::log_zero_atr_warning(processed_data_result.curr.timestamp.empty() ? "UNKNOWN" : processed_data_result.curr.timestamp);
        return;
    }
    
    std::lock_guard<std::mutex> state_lock(snapshot_state.state_mutex);
    
    if (processed_data_result.curr.open_price > 0.0 && (processed_data_result.curr.high_price == 0.0 || processed_data_result.curr.low_price == 0.0 || processed_data_result.curr.close_price == 0.0)) {
        MarketDataThreadLogs::log_thread_loop_exception("ProcessedData has incomplete bar data - O:" + 
            std::to_string(processed_data_result.curr.open_price) + " H:" + std::to_string(processed_data_result.curr.high_price) + 
            " L:" + std::to_string(processed_data_result.curr.low_price) + " C:" + std::to_string(processed_data_result.curr.close_price));
    }
    
    snapshot_state.market_snapshot.atr = processed_data_result.atr;
    snapshot_state.market_snapshot.avg_atr = processed_data_result.avg_atr;
    snapshot_state.market_snapshot.avg_vol = processed_data_result.avg_vol;
    
    snapshot_state.market_snapshot.curr.open_price = processed_data_result.curr.open_price;
    snapshot_state.market_snapshot.curr.high_price = processed_data_result.curr.high_price;
    snapshot_state.market_snapshot.curr.low_price = processed_data_result.curr.low_price;
    snapshot_state.market_snapshot.curr.close_price = processed_data_result.curr.close_price;
    snapshot_state.market_snapshot.curr.volume = processed_data_result.curr.volume;
    snapshot_state.market_snapshot.curr.timestamp = processed_data_result.curr.timestamp;
    
    snapshot_state.market_snapshot.prev.open_price = processed_data_result.prev.open_price;
    snapshot_state.market_snapshot.prev.high_price = processed_data_result.prev.high_price;
    snapshot_state.market_snapshot.prev.low_price = processed_data_result.prev.low_price;
    snapshot_state.market_snapshot.prev.close_price = processed_data_result.prev.close_price;
    snapshot_state.market_snapshot.prev.volume = processed_data_result.prev.volume;
    snapshot_state.market_snapshot.prev.timestamp = processed_data_result.prev.timestamp;
    
    snapshot_state.market_snapshot.oldest_bar_timestamp = processed_data_result.oldest_bar_timestamp;
    
    if (snapshot_state.market_snapshot.curr.open_price > 0.0 && (snapshot_state.market_snapshot.curr.high_price == 0.0 || snapshot_state.market_snapshot.curr.low_price == 0.0 || snapshot_state.market_snapshot.curr.close_price == 0.0)) {
        MarketDataThreadLogs::log_thread_loop_exception("Snapshot has incomplete bar data after copy - O:" + 
            std::to_string(snapshot_state.market_snapshot.curr.open_price) + " H:" + std::to_string(snapshot_state.market_snapshot.curr.high_price) + 
            " L:" + std::to_string(snapshot_state.market_snapshot.curr.low_price) + " C:" + std::to_string(snapshot_state.market_snapshot.curr.close_price));
    }
    
    snapshot_state.has_market_flag.store(true);
    
    auto current_timestamp = std::chrono::steady_clock::now();
    snapshot_state.market_data_timestamp.store(current_timestamp);
    snapshot_state.market_data_fresh_flag.store(true);
    
    snapshot_state.data_condition_variable.notify_all();
}

void MarketDataCoordinator::process_market_data_iteration(const std::string& symbol, MarketDataSnapshotState& snapshot_state, std::chrono::steady_clock::time_point& last_bar_log_time, Bar& previous_bar) {
    try {

        std::vector<Bar> historical_bars_for_logging;
        ProcessedData computed_data = fetch_and_process_market_data(symbol, historical_bars_for_logging);
        
        if (computed_data.atr == 0.0) {
            MarketDataThreadLogs::log_zero_atr_warning(symbol);
            return;
        }
        
        update_shared_market_snapshot(computed_data, snapshot_state);
        
        // Log bars to CSV immediately after successful processing for validation
        // This ensures we log the bars we're actually using for trading decisions
        try {
            auto* logging_context = AlpacaTrader::Logging::get_logging_context();
            if (logging_context && logging_context->csv_bars_logger && !historical_bars_for_logging.empty()) {
                std::string current_timestamp = TimeUtils::get_current_human_readable_time();
                
                // Log the latest bar (the one we're using for trading decisions)
                const auto& latest_bar = historical_bars_for_logging.back();
                std::string bar_timestamp = latest_bar.timestamp.empty() ? current_timestamp : latest_bar.timestamp;
                
                logging_context->csv_bars_logger->log_bar(
                    current_timestamp,  // System timestamp
                    symbol,
                    latest_bar,
                    computed_data.atr,
                    computed_data.avg_atr,
                    computed_data.avg_vol
                );
            }
        } catch (const std::exception& csv_log_exception) {
            MarketDataThreadLogs::log_csv_logging_error(symbol, "Exception logging bar to CSV: " + std::string(csv_log_exception.what()));
        } catch (...) {
            MarketDataThreadLogs::log_csv_logging_error(symbol, "Unknown exception logging bar to CSV");
        }
        
        MarketDataValidator& validator = market_data_manager.get_market_data_validator();
        const SystemConfig& config = market_data_manager.get_config();
        API::ApiManager& api_manager = market_data_manager.get_api_manager();
        MarketDataThreadLogs::process_csv_logging_if_needed(computed_data, historical_bars_for_logging, validator, symbol, config.timing, api_manager, last_bar_log_time, previous_bar);
        
    } catch (const std::exception& exception_error) {
        MarketDataThreadLogs::log_thread_loop_exception("Error in process_market_data_iteration: " + std::string(exception_error.what()));
    } catch (...) {
        MarketDataThreadLogs::log_thread_loop_exception("Unknown error in process_market_data_iteration");
    }
}

} // namespace Core
} // namespace AlpacaTrader

