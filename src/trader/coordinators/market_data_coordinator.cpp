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

// Static variables for throttling logs when waiting for data
static std::chrono::steady_clock::time_point last_waiting_data_log_time = std::chrono::steady_clock::time_point{};
static constexpr int waiting_data_log_interval_seconds = 5; // Log every 5 seconds when waiting for data

MarketDataCoordinator::MarketDataCoordinator(MarketDataManager& market_data_manager_ref)
    : market_data_manager(market_data_manager_ref) {}

void MarketDataCoordinator::initialize_mth_ts_components() {
    try {
        const SystemConfig& config = market_data_manager.get_config();

        // Initialize MTH-TS components if enabled
        if (config.strategy.mth_ts_enabled) {
            API::ApiManager& api_manager = market_data_manager.get_api_manager();

            if (api_manager.is_crypto_symbol(config.strategy.symbol)) {
                try {
                    API::PolygonCryptoClient* polygon_client = api_manager.get_polygon_crypto_client();
                    if (polygon_client) {
                        try {
                            polygon_client->initialize_multi_timeframe_manager(config);
                            log_message("MTH-TS MultiTimeframeManager initialized for symbol: " + config.strategy.symbol, "");
                        } catch (const std::exception& mth_init_exception_error) {
                            std::ostringstream mth_init_error_stream;
                            mth_init_error_stream << "CRITICAL: Failed to initialize MTH-TS MultiTimeframeManager: " << mth_init_exception_error.what();
                            log_message(mth_init_error_stream.str(), "");
                            throw std::runtime_error("MTH-TS MultiTimeframeManager initialization failed: " + std::string(mth_init_exception_error.what()));
                        } catch (...) {
                            log_message("CRITICAL: Unknown error initializing MTH-TS MultiTimeframeManager", "");
                            throw std::runtime_error("Unknown error initializing MTH-TS MultiTimeframeManager");
                        }
                    } else {
                        log_message("CRITICAL: PolygonCryptoClient not available for MTH-TS initialization", "");
                        throw std::runtime_error("PolygonCryptoClient not available for MTH-TS initialization");
                    }
                } catch (const std::exception& polygon_exception_error) {
                    std::ostringstream polygon_error_stream;
                    polygon_error_stream << "CRITICAL: Failed to get PolygonCryptoClient: " << polygon_exception_error.what();
                    log_message(polygon_error_stream.str(), "");
                    throw std::runtime_error("PolygonCryptoClient access failed: " + std::string(polygon_exception_error.what()));
                } catch (...) {
                    log_message("CRITICAL: Unknown error accessing PolygonCryptoClient", "");
                    throw std::runtime_error("Unknown error accessing PolygonCryptoClient");
                }
            }
        }
    } catch (const std::exception& e) {
        std::ostringstream error_stream;
        error_stream << "Failed to initialize MTH-TS components: " << e.what();
        log_message(error_stream.str(), "");
        throw;
    }
}

ProcessedData MarketDataCoordinator::fetch_and_process_market_data(const std::string& trading_symbol, std::vector<Bar>& historical_bars_output) {
    // Validate symbol matches MarketDataManager's configured symbol
    const SystemConfig& manager_config = market_data_manager.get_config();
    if (!trading_symbol.empty() && trading_symbol != manager_config.strategy.symbol) {
        MarketDataThreadLogs::log_thread_loop_exception("Symbol mismatch: requested " + trading_symbol + " but manager configured for " + manager_config.strategy.symbol);
    }
    
    try {
        // MarketDataManager fetches bars internally and returns them to avoid duplicate fetching
        auto fetch_result = market_data_manager.fetch_and_process_market_data();
        
        ProcessedData processed_data = fetch_result.first;
        historical_bars_output = fetch_result.second;
        
        // Check if we have valid price data
        bool has_valid_price_data = processed_data.curr.close_price > 0.0 && 
                                    processed_data.curr.open_price > 0.0 &&
                                    processed_data.curr.high_price > 0.0 &&
                                    processed_data.curr.low_price > 0.0;
        
        // Throttle logging when waiting for data - only log every N seconds
        auto current_time = std::chrono::steady_clock::now();
        bool should_log_verbose = true;
        
        if (!has_valid_price_data) {
            // Check if enough time has passed since last log
            if (last_waiting_data_log_time != std::chrono::steady_clock::time_point{}) {
                auto time_since_last_log = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_waiting_data_log_time).count();
                should_log_verbose = (time_since_last_log >= waiting_data_log_interval_seconds);
            }
            
            if (should_log_verbose) {
                last_waiting_data_log_time = current_time;
            }
        } else {
            // We have valid data, reset the throttling timer and always log
            last_waiting_data_log_time = std::chrono::steady_clock::time_point{};
            should_log_verbose = true;
        }
        
        // Only log verbose messages when we should (either have data or throttling allows it)
        if (should_log_verbose) {
            // Log market data fetch start
            MarketDataLogs::log_market_data_fetch_table(trading_symbol, manager_config.logging.log_file);
            
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
        }
        
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

void MarketDataCoordinator::update_shared_market_snapshot(const ProcessedData& processed_data_result, MarketDataSnapshotState& snapshot_state, const std::string& symbol, size_t bars_available_count) {
    // CRITICAL: Do not update snapshot with invalid price data (all zeros)
    // This prevents trading logic from receiving zero prices
    bool has_valid_price_data = processed_data_result.curr.close_price > 0.0 && 
                                processed_data_result.curr.open_price > 0.0 &&
                                processed_data_result.curr.high_price > 0.0 &&
                                processed_data_result.curr.low_price > 0.0;
    
    bool atr_is_zero = (processed_data_result.atr == 0.0);
    
    if (!has_valid_price_data) {
        // Throttle logging when waiting for data - only log every N seconds
        auto current_time = std::chrono::steady_clock::now();
        bool should_log_insufficient_data = true;
        
        if (last_waiting_data_log_time != std::chrono::steady_clock::time_point{}) {
            auto time_since_last_log = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_waiting_data_log_time).count();
            should_log_insufficient_data = (time_since_last_log >= waiting_data_log_interval_seconds);
        } else {
            // First time logging, update the timestamp
            last_waiting_data_log_time = current_time;
        }
        
        if (should_log_insufficient_data) {
            // Use condensed table format for insufficient data scenarios
            const SystemConfig& config = market_data_manager.get_config();
            int bars_required_count = config.strategy.bars_to_fetch_for_calculations;
            MarketDataThreadLogs::log_insufficient_data_condensed(
                symbol,
                atr_is_zero,
                true,
                processed_data_result.curr.close_price,
                processed_data_result.curr.open_price,
                processed_data_result.curr.high_price,
                processed_data_result.curr.low_price,
                bars_available_count,
                bars_required_count
            );
            last_waiting_data_log_time = current_time;
        }
        return;
    }
    
    // We have valid data, reset the throttling timer
    last_waiting_data_log_time = std::chrono::steady_clock::time_point{};
    
    // SPECIAL HANDLING FOR MTH-TS: If snapshot already has valid ATR but processing gives zero ATR,
    // this indicates MTH-TS data is being overwritten by insufficient accumulator data
    if (atr_is_zero && snapshot_state.market_snapshot.atr > 0.0) {
        // MTH-TS data is already in snapshot, skip update with zero ATR data
        MarketDataThreadLogs::log_zero_atr_warning(symbol + " (MTH-TS data preserved)");
        return;
    }

    // Handle MTH-TS sentinel values: -1.0 indicates ATR should be calculated by MTH-TS processing
    if (processed_data_result.atr == -1.0) {
        // This is expected for MTH-TS - ATR will be set by the MTH-TS processing in trading coordinator
        // Don't update the snapshot with sentinel values
        return;
    }
    
    if (atr_is_zero) {
        // ATR is zero but price data is valid - log warning but proceed (snapshot will update)
        MarketDataThreadLogs::log_zero_atr_warning(symbol);
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

        // LOG MOST RECENT BARS FROM ROLLING WINDOWS
        // The historical_bars_for_logging comes from websocket accumulator which may be stale
        // But the rolling windows (minute_bars, thirty_min_bars, daily_bars) are continuously updated
        // For logging, we want the most recent bars from these rolling windows, not the stale accumulator

        // Get fresh bars from the rolling windows instead of potentially stale accumulator
        std::vector<Bar> fresh_bars_for_logging;
        try {
            auto& api_mgr = market_data_manager.get_api_manager();
            auto* polygon_client = api_mgr.get_polygon_crypto_client();
            auto* mtf_manager = polygon_client ? polygon_client->get_multi_timeframe_manager() : nullptr;

            if (mtf_manager) {
                const auto& mtf_data = mtf_manager->get_multi_timeframe_data();

                // CRITICAL FIX: Log 1-second bars for real-time granularity, not 1-minute aggregated bars
                // This ensures each logged bar represents a unique 1-second window, not stale 1-minute data
                // Convert MultiTimeframeBar to Bar for CSV logging compatibility
                if (!mtf_data.second_bars.empty()) {
                    const auto& mtf_bar = mtf_data.second_bars.back();
                    Bar bar;
                    bar.open_price = mtf_bar.open_price;
                    bar.high_price = mtf_bar.high_price;
                    bar.low_price = mtf_bar.low_price;
                    bar.close_price = mtf_bar.close_price;
                    bar.volume = mtf_bar.volume;
                    bar.timestamp = mtf_bar.timestamp;
                    fresh_bars_for_logging.push_back(bar);
                } else if (!mtf_data.minute_bars.empty()) {
                    const auto& mtf_bar = mtf_data.minute_bars.back();
                    Bar bar;
                    bar.open_price = mtf_bar.open_price;
                    bar.high_price = mtf_bar.high_price;
                    bar.low_price = mtf_bar.low_price;
                    bar.close_price = mtf_bar.close_price;
                    bar.volume = mtf_bar.volume;
                    bar.timestamp = mtf_bar.timestamp;
                    fresh_bars_for_logging.push_back(bar);
                } else if (!mtf_data.thirty_min_bars.empty()) {
                    const auto& mtf_bar = mtf_data.thirty_min_bars.back();
                    Bar bar;
                    bar.open_price = mtf_bar.open_price;
                    bar.high_price = mtf_bar.high_price;
                    bar.low_price = mtf_bar.low_price;
                    bar.close_price = mtf_bar.close_price;
                    bar.volume = mtf_bar.volume;
                    bar.timestamp = mtf_bar.timestamp;
                    fresh_bars_for_logging.push_back(bar);
                } else if (!mtf_data.daily_bars.empty()) {
                    const auto& mtf_bar = mtf_data.daily_bars.back();
                    Bar bar;
                    bar.open_price = mtf_bar.open_price;
                    bar.high_price = mtf_bar.high_price;
                    bar.low_price = mtf_bar.low_price;
                    bar.close_price = mtf_bar.close_price;
                    bar.volume = mtf_bar.volume;
                    bar.timestamp = mtf_bar.timestamp;
                    fresh_bars_for_logging.push_back(bar);
                } else if (!historical_bars_for_logging.empty()) {
                    // Fallback to accumulator bars if rolling windows are empty
                    fresh_bars_for_logging.push_back(historical_bars_for_logging.back());
                }

                // Replace historical_bars_for_logging with fresh rolling window bars
                if (!fresh_bars_for_logging.empty()) {
                    historical_bars_for_logging = std::move(fresh_bars_for_logging);
                }
            }
        } catch (const std::exception& rolling_window_error) {
            // Log error but continue with original bars
            LOG_THREAD_CONTENT("Failed to get fresh bars from rolling windows: " + std::string(rolling_window_error.what()));
        }
        
        // Update snapshot - logging for insufficient data is handled inside update_shared_market_snapshot
        update_shared_market_snapshot(computed_data, snapshot_state, symbol, historical_bars_for_logging.size());
        
        const SystemConfig& config_ref = market_data_manager.get_config();

        // Log bars to CSV IMMEDIATELY as they arrive - no throttling for real-time 1-second bars
        // For MTH-TS strategy, we need every single 1-second bar logged for analysis
        // Deduplication is handled by checking if bar timestamp/data is different from previous
        auto current_time = std::chrono::steady_clock::now();

        if (true) {  // Always attempt to log - deduplication logic below prevents duplicates
        try {
            auto* logging_context = AlpacaTrader::Logging::get_logging_context();
            if (logging_context && logging_context->csv_bars_logger && !historical_bars_for_logging.empty()) {
                std::string current_timestamp = TimeUtils::get_current_human_readable_time();
                
                // Log the latest bar (the one we're using for trading decisions)
                const auto& latest_bar = historical_bars_for_logging.back();
                
                // CRITICAL: Only log if this is a NEW bar (different timestamp from previous)
                // This prevents duplicate logging of the same 1-second bar across multiple trading cycles
                bool is_new_bar = (previous_bar.timestamp.empty() || latest_bar.timestamp != previous_bar.timestamp);
                
                if (is_new_bar) {
                    std::string bar_timestamp = latest_bar.timestamp.empty() ? current_timestamp : latest_bar.timestamp;
                    
                    // For MTH-TS, use real-time ATR from multi-timeframe indicators, not historical ATR sentinel value
                    double realtime_atr = computed_data.atr;
                    double realtime_avg_atr = computed_data.avg_atr;
                    
                    try {
                        auto& api_mgr = market_data_manager.get_api_manager();
                        auto* polygon_client = api_mgr.get_polygon_crypto_client();
                        auto* mtf_mgr = polygon_client ? polygon_client->get_multi_timeframe_manager() : nullptr;
                        
                        if (mtf_mgr) {
                            const auto& mtf_data = mtf_mgr->get_multi_timeframe_data();
                            // Use 1-second timeframe ATR for most current volatility measurement
                            if (mtf_data.second_indicators.atr > 0.0) {
                                realtime_atr = mtf_data.second_indicators.atr;
                                realtime_avg_atr = mtf_data.second_indicators.atr; // Can calculate moving average if needed
                            }
                        }
                    } catch (...) {
                        // If we can't get MTH-TS ATR, use the historical ATR value
                    }
                    
                    logging_context->csv_bars_logger->log_bar(
                        current_timestamp,  // System timestamp
                        symbol,
                        latest_bar,
                        realtime_atr,
                        realtime_avg_atr,
                        computed_data.avg_vol
                    );

                    // Update previous_bar to track what we just logged
                    previous_bar = latest_bar;
                    last_bar_log_time = current_time;
                }
            }
        } catch (const std::exception& csv_log_exception) {
            MarketDataThreadLogs::log_csv_logging_error(symbol, "Exception logging bar to CSV: " + std::string(csv_log_exception.what()));
        } catch (...) {
            MarketDataThreadLogs::log_csv_logging_error(symbol, "Unknown exception logging bar to CSV");
            }
        }
        
        MarketDataThreadLogs::process_csv_logging_if_needed(computed_data, historical_bars_for_logging, symbol, config_ref.timing, config_ref.logging, last_bar_log_time, previous_bar);
        
    } catch (const std::exception& exception_error) {
        MarketDataThreadLogs::log_thread_loop_exception("Error in process_market_data_iteration: " + std::string(exception_error.what()));
    } catch (...) {
        MarketDataThreadLogs::log_thread_loop_exception("Unknown error in process_market_data_iteration");
    }
}

} // namespace Core
} // namespace AlpacaTrader

