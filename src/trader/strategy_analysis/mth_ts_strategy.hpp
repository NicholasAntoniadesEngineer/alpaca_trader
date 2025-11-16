#ifndef MTH_TS_STRATEGY_HPP
#define MTH_TS_STRATEGY_HPP

#include "configs/system_config.hpp"
#include "trader/data_structures/data_structures.hpp"
#include "api/general/api_manager.hpp"
#include "api/polygon/polygon_crypto_client.hpp"
#include "trader/trading_logic/order_execution_logic.hpp"
#include "trader/market_data/multi_timeframe_manager.hpp"
#include "logging/logger/logging_macros.hpp"

using AlpacaTrader::Config::SystemConfig;
using AlpacaTrader::Logging::log_message;

namespace AlpacaTrader {
namespace Core {

// MTH-TS Analysis Result Structure
struct MthTsAnalysisResult {
    SignalDecision signal_decision;
    MthTsTimeframeAnalysis timeframe_status;

    MthTsAnalysisResult() : signal_decision(), timeframe_status() {}
};

class MthTsStrategy {
public:
    // Local enum to avoid circular dependency with OrderExecutionLogic
    enum class OrderSide { Buy, Sell };

    MthTsStrategy(const SystemConfig& config, API::ApiManager& api_manager, int current_position_quantity, const ProcessedData& processed_data);

    // MTH-TS evaluation methods
    SignalDecision evaluate_signals(const ProcessedData& processed_data, API::ApiManager* api_manager);
    std::string get_strategy_name() const { return "MTH-TS"; }
    bool is_enabled(const SystemConfig& config) const { return config.strategy.mth_ts_enabled; }
    bool supports_asset_type(bool is_crypto) const { return is_crypto; } // MTH-TS only for crypto

    // Legacy method for backward compatibility (internal use only)
    MthTsAnalysisResult evaluate_mth_ts_strategy();

    // Individual timeframe analysis methods
    bool evaluate_daily_level();
    bool evaluate_thirty_min_level();
    bool evaluate_one_min_level();
    bool evaluate_one_sec_level();

    // Hybrid evaluation helpers
    double compute_propagation_score(bool one_min_aligned, bool one_sec_aligned);

    // Order execution logic
    OrderExecutionResult execute_mth_ts_order(OrderSide order_side);
    PositionClosureResult check_and_close_positions();

    // Reversal detection
    bool detect_reversal_signal();

    // Access to multi-timeframe data
    const MultiTimeframeData& get_multi_timeframe_data() const;
    MultiTimeframeManager* get_multi_timeframe_manager() const;

private:
    const SystemConfig& config;
    API::ApiManager& api_manager;
    int current_position_quantity;
    const ProcessedData& processed_data;
    MultiTimeframeManager* multi_timeframe_manager;

    // Comprehensive technical analysis for any timeframe
    struct TimeframeAnalysisResult {
        bool atr_filter_pass;
        bool volume_filter_pass;
        bool doji_filter_pass;
        bool basic_buy_pattern;
        bool buy_high_condition;
        bool buy_low_condition;
        bool momentum_buy_signal;
        bool volume_buy_confirmation;
        bool volatility_buy_confirmation;
        bool consolidated_ready;
    };

    TimeframeAnalysisResult perform_comprehensive_technical_analysis(
        const MultiTimeframeBar& current_bar,
        const MultiTimeframeBar& previous_bar,
        double atr_value) const;

    // Helper methods
    double get_current_price() const;
    int get_current_position() const;
    bool is_position_open() const;
    void place_market_order(OrderSide side, double quantity);
    void place_limit_order(OrderSide side, double quantity, double price);
    void place_stop_limit_order(OrderSide side, double quantity, double stop_price, double limit_price);
    void close_all_positions();
};

// Free functions for compatibility with trading_logic.cpp
SignalDecision detect_trading_signals(const ProcessedData& processed_data_input, const SystemConfig& system_config, API::ApiManager* api_manager = nullptr);
FilterResult evaluate_trading_filters(const ProcessedData& processed_data_input, const SystemConfig& system_config);
std::pair<PositionSizing, SignalDecision> process_position_sizing(const PositionSizingProcessRequest& request);
ExitTargets compute_exit_targets(const ExitTargetsRequest& request);

} // namespace Core
} // namespace AlpacaTrader

#endif // MTH_TS_STRATEGY_HPP
