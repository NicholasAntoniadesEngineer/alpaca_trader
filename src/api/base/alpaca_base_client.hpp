#ifndef ALPACA_BASE_CLIENT_HPP
#define ALPACA_BASE_CLIENT_HPP

#include "core/threads/thread_register.hpp"

namespace AlpacaTrader {
namespace API {

using AlpacaClientConfig = AlpacaTrader::Config::AlpacaClientConfig;

class AlpacaBaseClient {
protected:
    const ApiConfig& api;
    const SessionConfig& session;
    const TimingConfig& timing;
    const LoggingConfig& logging;
    const TargetConfig& target;
    const OrdersConfig& orders;

public:
    explicit AlpacaBaseClient(const AlpacaClientConfig& cfg)
        : api(cfg.api), session(cfg.session), timing(cfg.timing), 
          logging(cfg.logging), target(cfg.target), orders(cfg.orders) {}
    
    virtual ~AlpacaBaseClient() {}
};

} // namespace API
} // namespace AlpacaTrader

#endif // ALPACA_BASE_CLIENT_HPP
