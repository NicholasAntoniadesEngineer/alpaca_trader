// alpaca_client.hpp
#ifndef ALPACA_CLIENT_HPP
#define ALPACA_CLIENT_HPP

#include "../configs/api_config.hpp"
#include "../configs/session_config.hpp"
#include "../configs/logging_config.hpp"
#include "../configs/target_config.hpp"
#include "../configs/timing_config.hpp"
#include "../data/data_structures.hpp"

class AlpacaClient {
private:
    const ApiConfig& api;
    const SessionConfig& session;
    const TimingConfig& timing;
    const LoggingConfig& logging;
    const TargetConfig& target;

public:
    AlpacaClient(const ApiConfig& apiCfg,
                 const SessionConfig& sessionCfg,
                 const LoggingConfig& loggingCfg,
                 const TargetConfig& targetCfg,
                 const TimingConfig& timingCfg)
        : api(apiCfg), session(sessionCfg), timing(timingCfg), logging(loggingCfg), target(targetCfg) {}

    // Pure API Interface Functions
    bool is_core_trading_hours() const;
    bool is_within_fetch_window() const;
    std::vector<Bar> get_recent_bars(const BarRequest& req) const;
    void place_bracket_order(const OrderRequest& req) const;
    void close_position(const ClosePositionRequest& req) const;
};

#endif // ALPACA_CLIENT_HPP
