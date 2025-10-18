/**
 * Market data collection and processing thread.
 * Fetches real-time market data for trading decisions.
 */
#include "market_data_thread.hpp"
#include "core/logging/async_logger.hpp"
#include "core/logging/csv_bars_logger.hpp"
#include "core/logging/startup_logs.hpp"
#include "core/logging/logging_macros.hpp"
#include "core/threads/thread_logic/platform/thread_control.hpp"
#include "core/trader/data/market_processing.hpp"
#include "core/utils/connectivity_manager.hpp"
#include "core/utils/time_utils.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>

// Using declarations for cleaner code
using AlpacaTrader::Threads::MarketDataThread;
using AlpacaTrader::Logging::set_log_thread_tag;
using AlpacaTrader::Logging::log_message;
using AlpacaTrader::Core::BarRequest;
using AlpacaTrader::Core::ProcessedData;
using AlpacaTrader::API::AlpacaClient;

void AlpacaTrader::Threads::MarketDataThread::operator()() {
    set_log_thread_tag("MARKET");
    
    try {
        // Wait for main thread to complete priority setup
        std::this_thread::sleep_for(std::chrono::milliseconds(timing.thread_startup_sequence_delay_milliseconds));
        
        // Start the market data collection loop
        market_data_loop();
    } catch (const std::exception& e) {
        log_message("MarketDataThread exception: " + std::string(e.what()), "trading_system.log");
    } catch (...) {
        log_message("MarketDataThread unknown exception", "trading_system.log");
    }
}

void AlpacaTrader::Threads::MarketDataThread::market_data_loop() {
    try {
        while (running.load()) {
            try {
                if (!allow_fetch_ptr || !allow_fetch_ptr->load()) {
                    std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
                    continue;
                }

                fetch_and_process_market_data();

                if (iteration_counter) {
                    iteration_counter->fetch_add(1);
                }

                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
            } catch (const std::exception& e) {
                log_message("MarketDataThread loop iteration exception: " + std::string(e.what()), "trading_system.log");
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
            } catch (...) {
                log_message("MarketDataThread loop iteration unknown exception", "trading_system.log");
                // Continue running - don't exit the thread
                std::this_thread::sleep_for(std::chrono::seconds(timing.thread_market_data_poll_interval_sec));
            }
        }
    } catch (const std::exception& e) {
        log_message("MarketDataThread market_data_loop exception: " + std::string(e.what()), "trading_system.log");
    } catch (...) {
        log_message("MarketDataThread market_data_loop unknown exception", "trading_system.log");
    }
}

void AlpacaTrader::Threads::MarketDataThread::fetch_and_process_market_data() {
    LOG_THREAD_SECTION_HEADER("MARKET DATA FETCH - " + strategy.symbol);
    
    // First, get historical bars for ATR calculation
    int num_bars = strategy.bars_to_fetch_for_calculations + timing.historical_data_buffer_size;
    BarRequest br{strategy.symbol, num_bars};
    LOG_THREAD_CONTENT("Requesting " + std::to_string(num_bars) + " bars for ATR calculation");
    
    auto bars = client.get_recent_bars(br);
    LOG_THREAD_CONTENT("Received " + std::to_string(bars.size()) + " bars");
    
    // Use configurable ATR calculation bars instead of deprecated atr_calculation_period
    if (static_cast<int>(bars.size()) >= strategy.atr_calculation_bars + 2) {
        LOG_THREAD_CONTENT("Sufficient bars, computing indicators");
        
        // Compute indicators using the same implementation as Trader
        SystemConfig minimal_cfg;
        minimal_cfg.strategy = strategy;
        minimal_cfg.timing = timing;
        minimal_cfg.logging = LoggingConfig{};
        ProcessedData computed = AlpacaTrader::Core::MarketProcessing::compute_processed_data(bars, minimal_cfg);

        LOG_THREAD_CONTENT("ATR computed: " + std::to_string(computed.atr));
        LOG_THREAD_CONTENT("Current price: $" + std::to_string(computed.curr.c));

        if (computed.atr != 0.0) {
            LOG_THREAD_CONTENT("Updating market snapshot");
            update_market_snapshot(computed);

            // Check if we should log real-time quotes (every 5 seconds)
            auto now = std::chrono::steady_clock::now();
            auto time_since_last_log = std::chrono::duration_cast<std::chrono::seconds>(now - last_bar_log_time).count();
            bool should_log = last_bar_log_time == std::chrono::steady_clock::time_point{} || // First log
                             time_since_last_log >= 5; // Or at least 5 seconds have passed

            LOG_THREAD_CONTENT("CSV Logger available: " + std::string(AlpacaTrader::Logging::g_csv_bars_logger ? "YES" : "NO"));
            LOG_THREAD_CONTENT("Should log: " + std::string(should_log ? "YES" : "NO") + " (time since last: " + std::to_string(time_since_last_log) + "s)");

            if (should_log) {
                try {
                    if (AlpacaTrader::Logging::g_csv_bars_logger) {
                        std::string timestamp = TimeUtils::get_current_human_readable_time();

                        // Get real-time quotes for current market data
                        LOG_THREAD_CONTENT("Fetching real-time quotes for " + strategy.symbol);
                        auto quote_data = client.get_realtime_quotes(strategy.symbol);
                        LOG_THREAD_CONTENT("Quote data received - Mid: $" + std::to_string(quote_data.mid_price) + ", Timestamp: " + quote_data.timestamp);
                        
                        // Check if quote data is fresh (within last 2 minutes)
                        bool is_quote_fresh = false;
                        if (quote_data.mid_price > 0.0 && !quote_data.timestamp.empty()) {
                            // Parse the timestamp to check if it's recent
                            try {
                                // Convert quote timestamp to time_t for comparison
                                std::tm tm = {};
                                std::istringstream ss(quote_data.timestamp);
                                ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
                                if (!ss.fail()) {
                                    auto quote_time = std::mktime(&tm);
                                    auto current_time = std::time(nullptr);
                                    auto time_diff = current_time - quote_time;
                                    is_quote_fresh = (time_diff < 120); // 2 minutes
                                    
                                    if (!is_quote_fresh) {
                                        LOG_THREAD_CONTENT("⚠️  CRYPTO DATA WARNING: Quote data is stale (age: " + std::to_string(time_diff) + "s = " + std::to_string(time_diff/3600) + "h), using bar data");
                                        LOG_THREAD_CONTENT("⚠️  NOTE: Alpaca crypto data appears to be delayed/historical only, not real-time");
                                    }
                                }
                            } catch (const std::exception& e) {
                                LOG_THREAD_CONTENT("Error parsing quote timestamp: " + std::string(e.what()));
                            }
                        }
                        
                        if (quote_data.mid_price > 0.0 && is_quote_fresh) {
                            // Log real-time quote data
                            AlpacaTrader::Logging::g_csv_bars_logger->log_quote(
                                quote_data, strategy.symbol, timestamp, computed.atr, computed.avg_atr, computed.avg_vol
                            );
                            LOG_THREAD_CONTENT("Logged FRESH real-time quote data to CSV (Price: $" + std::to_string(quote_data.mid_price) + ")");
                        } else {
                            // Fallback to bar data if quotes are stale or unavailable
                            // Only log bars if we haven't logged them before (prevent duplicate historical data)
                            if (previous_bar.t.empty() || bars.back().t != previous_bar.t) {
                                LOG_THREAD_CONTENT("Logging " + std::to_string(bars.size()) + " bars to CSV (quotes stale/unavailable)");
                                // Log ALL bars that were fetched, not just the last one
                                // Use individual bar timestamps instead of current system time
                                for (const auto& bar : bars) {
                                    std::string bar_timestamp = bar.t.empty() ? timestamp : bar.t;
                                    AlpacaTrader::Logging::g_csv_bars_logger->log_bar(
                                        bar, strategy.symbol, bar_timestamp, computed.atr, computed.avg_atr, computed.avg_vol
                                    );
                                }
                                LOG_THREAD_CONTENT("Successfully logged " + std::to_string(bars.size()) + " bars to CSV");
                                // Update previous_bar to track the latest bar we logged
                                previous_bar = bars.back();
                            } else {
                                LOG_THREAD_CONTENT("Skipping bar logging - same historical data (latest bar: " + bars.back().t + ")");
                            }
                        }
                        last_bar_log_time = now;
                    }
                } catch (const std::exception& e) {
                    LOG_THREAD_CONTENT("CSV logging error: " + std::string(e.what()));
                } catch (...) {
                    LOG_THREAD_CONTENT("Unknown CSV logging error");
                }
            } else {
                LOG_THREAD_CONTENT("Skipping CSV logging - too soon since last log (" + std::to_string(time_since_last_log) + "s, need 5s)");
            }
        } else {
            LOG_THREAD_CONTENT("ATR is zero, not updating snapshot");
        }
    } else {
        LOG_THREAD_CONTENT("Insufficient bars (" + std::to_string(bars.size()) + " < " + std::to_string(strategy.atr_calculation_bars + 2) + ")");
    }
    
    LOG_THREAD_SECTION_FOOTER();
}

void AlpacaTrader::Threads::MarketDataThread::update_market_snapshot(const ProcessedData& computed) {
    std::lock_guard<std::mutex> lock(state_mtx);

    market_snapshot.atr = computed.atr;
    market_snapshot.avg_atr = computed.avg_atr;
    market_snapshot.avg_vol = computed.avg_vol;
    market_snapshot.curr = computed.curr;
    market_snapshot.prev = computed.prev;
    has_market.store(true);

    // Update data freshness timestamp
    auto now = std::chrono::steady_clock::now();
    market_data_timestamp.store(now);
    market_data_fresh.store(true);
    
    data_cv.notify_all();
}


