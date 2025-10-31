#ifndef LOGGER_STRUCTURES_HPP
#define LOGGER_STRUCTURES_HPP

#include <string>
#include <vector>
#include <tuple>

namespace AlpacaTrader {
namespace Logging {

struct ComprehensiveOrderExecutionRequest {
    std::string order_type;
    std::string side;
    int quantity;
    double current_price;
    double atr;
    int position_quantity;
    double risk_amount;
    double stop_loss;
    double take_profit;
    std::string symbol;
    std::string function_name;
    
    ComprehensiveOrderExecutionRequest(const std::string& order_type_param, const std::string& side_param,
                                     int quantity_param, double current_price_param, double atr_param,
                                     int position_qty_param, double risk_amount_param,
                                     double stop_loss_param, double take_profit_param,
                                     const std::string& symbol_param, const std::string& function_name_param)
        : order_type(order_type_param), side(side_param), quantity(quantity_param),
          current_price(current_price_param), atr(atr_param), position_quantity(position_qty_param),
          risk_amount(risk_amount_param), stop_loss(stop_loss_param), take_profit(take_profit_param),
          symbol(symbol_param), function_name(function_name_param) {}
};

struct ComprehensiveApiResponseRequest {
    std::string order_id;
    std::string status;
    std::string side;
    std::string quantity;
    std::string order_class;
    std::string position_intent;
    std::string created_at;
    std::string filled_at;
    std::string filled_quantity;
    std::string filled_avg_price;
    std::string error_code;
    std::string error_message;
    std::string available_quantity;
    std::string existing_quantity;
    std::string held_for_orders;
    std::string related_orders;
    
    ComprehensiveApiResponseRequest(const std::string& order_id_param, const std::string& status_param,
                                   const std::string& side_param, const std::string& quantity_param,
                                   const std::string& order_class_param, const std::string& position_intent_param,
                                   const std::string& created_at_param, const std::string& filled_at_param,
                                   const std::string& filled_qty_param, const std::string& filled_avg_price_param,
                                   const std::string& error_code_param, const std::string& error_message_param,
                                   const std::string& available_qty_param, const std::string& existing_qty_param,
                                   const std::string& held_for_orders_param, const std::string& related_orders_param)
        : order_id(order_id_param), status(status_param), side(side_param), quantity(quantity_param),
          order_class(order_class_param), position_intent(position_intent_param),
          created_at(created_at_param), filled_at(filled_at_param), filled_quantity(filled_qty_param),
          filled_avg_price(filled_avg_price_param), error_code(error_code_param), error_message(error_message_param),
          available_quantity(available_qty_param), existing_quantity(existing_qty_param),
          held_for_orders(held_for_orders_param), related_orders(related_orders_param) {}
};

struct FinancialSummaryTableRequest {
    double equity;
    double last_equity;
    double cash;
    double buying_power;
    double long_market_value;
    double short_market_value;
    double initial_margin;
    double maintenance_margin;
    double sma;
    int day_trade_count;
    double regt_buying_power;
    double day_trading_buying_power;
    
    FinancialSummaryTableRequest(double equity_param, double last_equity_param, double cash_param,
                                double buying_power_param, double long_market_value_param,
                                double short_market_value_param, double initial_margin_param,
                                double maintenance_margin_param, double sma_param, int day_trade_count_param,
                                double regt_buying_power_param, double day_trading_buying_power_param)
        : equity(equity_param), last_equity(last_equity_param), cash(cash_param),
          buying_power(buying_power_param), long_market_value(long_market_value_param),
          short_market_value(short_market_value_param), initial_margin(initial_margin_param),
          maintenance_margin(maintenance_margin_param), sma(sma_param), day_trade_count(day_trade_count_param),
          regt_buying_power(regt_buying_power_param), day_trading_buying_power(day_trading_buying_power_param) {}
};

struct DecisionSummaryTableRequest {
    std::string symbol;
    double price;
    bool buy_signal;
    bool sell_signal;
    bool atr_pass;
    bool volume_pass;
    bool doji_pass;
    double exposure_percentage;
    double atr_ratio;
    double volume_ratio;
    
    DecisionSummaryTableRequest(const std::string& symbol_param, double price_param,
                               bool buy_signal_param, bool sell_signal_param,
                               bool atr_pass_param, bool volume_pass_param, bool doji_pass_param,
                               double exposure_pct_param, double atr_ratio_param, double volume_ratio_param)
        : symbol(symbol_param), price(price_param), buy_signal(buy_signal_param),
          sell_signal(sell_signal_param), atr_pass(atr_pass_param), volume_pass(volume_pass_param),
          doji_pass(doji_pass_param), exposure_percentage(exposure_pct_param),
          atr_ratio(atr_ratio_param), volume_ratio(volume_ratio_param) {}
};

struct TradingConditionsTableRequest {
    double daily_pnl_percentage;
    double daily_loss_limit;
    double daily_profit_target;
    double exposure_percentage;
    double max_exposure_percentage;
    bool conditions_met;
    
    TradingConditionsTableRequest(double daily_pnl_pct_param, double daily_loss_limit_param,
                                 double daily_profit_target_param, double exposure_pct_param,
                                 double max_exposure_pct_param, bool conditions_met_param)
        : daily_pnl_percentage(daily_pnl_pct_param), daily_loss_limit(daily_loss_limit_param),
          daily_profit_target(daily_profit_target_param), exposure_percentage(exposure_pct_param),
          max_exposure_percentage(max_exposure_pct_param), conditions_met(conditions_met_param) {}
};

struct FiltersTableRequest {
    bool atr_pass;
    double atr_value;
    double atr_threshold;
    bool volume_pass;
    double volume_ratio;
    double volume_threshold;
    bool doji_pass;
    
    FiltersTableRequest(bool atr_pass_param, double atr_value_param, double atr_threshold_param,
                       bool volume_pass_param, double volume_ratio_param, double volume_threshold_param,
                       bool doji_pass_param)
        : atr_pass(atr_pass_param), atr_value(atr_value_param), atr_threshold(atr_threshold_param),
          volume_pass(volume_pass_param), volume_ratio(volume_ratio_param),
          volume_threshold(volume_threshold_param), doji_pass(doji_pass_param) {}
};

struct ExitTargetsTableRequest {
    std::string side;
    double price;
    double risk;
    double risk_reward_ratio;
    double stop_loss;
    double take_profit;
    
    ExitTargetsTableRequest(const std::string& side_param, double price_param, double risk_param,
                           double rr_ratio_param, double stop_loss_param, double take_profit_param)
        : side(side_param), price(price_param), risk(risk_param),
          risk_reward_ratio(rr_ratio_param), stop_loss(stop_loss_param), take_profit(take_profit_param) {}
};

struct PositionSizingTableRequest {
    double risk_amount;
    int quantity;
    double buying_power;
    double current_price;
    
    PositionSizingTableRequest(double risk_amount_param, int quantity_param,
                              double buying_power_param, double current_price_param)
        : risk_amount(risk_amount_param), quantity(quantity_param),
          buying_power(buying_power_param), current_price(current_price_param) {}
};

struct SizingAnalysisTableRequest {
    int risk_based_quantity;
    int exposure_based_quantity;
    int max_value_quantity;
    int buying_power_quantity;
    int final_quantity;
    
    SizingAnalysisTableRequest(int risk_based_qty_param, int exposure_based_qty_param,
                              int max_value_qty_param, int buying_power_qty_param, int final_qty_param)
        : risk_based_quantity(risk_based_qty_param), exposure_based_quantity(exposure_based_qty_param),
          max_value_quantity(max_value_qty_param), buying_power_quantity(buying_power_qty_param),
          final_quantity(final_qty_param) {}
};

struct InsufficientBuyingPowerRequest {
    double required_buying_power;
    double available_buying_power;
    int quantity;
    double current_price;
    
    InsufficientBuyingPowerRequest(double required_buying_power_param, double available_buying_power_param,
                                  int quantity_param, double current_price_param)
        : required_buying_power(required_buying_power_param), available_buying_power(available_buying_power_param),
          quantity(quantity_param), current_price(current_price_param) {}
};

struct PositionSizeWithBuyingPowerRequest {
    double risk_amount;
    int quantity;
    double buying_power;
    double current_price;
    
    PositionSizeWithBuyingPowerRequest(double risk_amount_param, int quantity_param,
                                      double buying_power_param, double current_price_param)
        : risk_amount(risk_amount_param), quantity(quantity_param),
          buying_power(buying_power_param), current_price(current_price_param) {}
};

struct CandleDataTableRequest {
    double open;
    double high;
    double low;
    double close;
    
    CandleDataTableRequest(double open_param, double high_param, double low_param, double close_param)
        : open(open_param), high(high_param), low(low_param), close(close_param) {}
};

struct CurrentPositionsTableRequest {
    int quantity;
    double current_value;
    double unrealized_pnl;
    double exposure_percentage;
    int open_orders;
    
    CurrentPositionsTableRequest(int quantity_param, double current_value_param,
                                double unrealized_pnl_param, double exposure_pct_param, int open_orders_param)
        : quantity(quantity_param), current_value(current_value_param),
          unrealized_pnl(unrealized_pnl_param), exposure_percentage(exposure_pct_param),
          open_orders(open_orders_param) {}
};

struct AccountOverviewTableRequest {
    std::string account_number;
    std::string status;
    std::string currency;
    bool pattern_day_trader;
    std::string created_date;
    
    AccountOverviewTableRequest(const std::string& account_number_param, const std::string& status_param,
                               const std::string& currency_param, bool pattern_day_trader_param,
                               const std::string& created_date_param)
        : account_number(account_number_param), status(status_param), currency(currency_param),
          pattern_day_trader(pattern_day_trader_param), created_date(created_date_param) {}
};

struct TraderStartupTableRequest {
    double initial_equity;
    double risk_per_trade;
    double risk_reward_ratio;
    int loop_interval;
    
    TraderStartupTableRequest(double initial_equity_param, double risk_per_trade_param,
                             double rr_ratio_param, int loop_interval_param)
        : initial_equity(initial_equity_param), risk_per_trade(risk_per_trade_param),
          risk_reward_ratio(rr_ratio_param), loop_interval(loop_interval_param) {}
};

} // namespace Logging
} // namespace AlpacaTrader

#endif // LOGGER_STRUCTURES_HPP

