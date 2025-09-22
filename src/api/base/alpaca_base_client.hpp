#ifndef ALPACA_BASE_CLIENT_HPP
#define ALPACA_BASE_CLIENT_HPP

#include "../../configs/api_config.hpp"
#include "../../configs/session_config.hpp"
#include "../../configs/logging_config.hpp"
#include "../../configs/target_config.hpp"
#include "../../configs/timing_config.hpp"
#include "../../configs/component_configs.hpp"

class AlpacaBaseClient {
protected:
    const ApiConfig& api;
    const SessionConfig& session;
    const TimingConfig& timing;
    const LoggingConfig& logging;
    const TargetConfig& target;

public:
    explicit AlpacaBaseClient(const AlpacaClientConfig& cfg)
        : api(cfg.api), session(cfg.session), timing(cfg.timing), 
          logging(cfg.logging), target(cfg.target) {}
    
    virtual ~AlpacaBaseClient() {}
};

#endif // ALPACA_BASE_CLIENT_HPP
