#ifndef ALPACA_BASE_CLIENT_HPP
#define ALPACA_BASE_CLIENT_HPP

#include "core/threads/thread_register.hpp"

namespace AlpacaTrader {
namespace API {

using AlpacaClientConfig = AlpacaTrader::Config::AlpacaClientConfig;

class AlpacaBaseClient {
protected:
    const AlpacaTrader::Config::ApiConfig& api;
    const TimingConfig& timing;
    const LoggingConfig& logging;
    const StrategyConfig& strategy;  // Now contains target, session, orders, risk, position, profit-taking

public:
    explicit AlpacaBaseClient(const AlpacaClientConfig& cfg)
        : api(cfg.api), timing(cfg.timing), logging(cfg.logging), strategy(cfg.strategy) {}

    virtual ~AlpacaBaseClient() {}
};

} // namespace API
} // namespace AlpacaTrader

#endif // ALPACA_BASE_CLIENT_HPP
