#ifndef TRADING_SYSTEM_FACTORY_HPP
#define TRADING_SYSTEM_FACTORY_HPP

#include "configs/system_config.hpp"
#include "api/general/api_manager.hpp"
#include "core/trader/data/account_manager.hpp"
#include "core/trader/trader.hpp"
#include "core/system/system_monitor.hpp"
#include "core/utils/connectivity_manager.hpp"
#include <memory>

namespace AlpacaTrader {
namespace Core {

class TradingSystemFactory {
public:
    struct TradingSystemComponents {
        std::unique_ptr<API::ApiManager> api_manager;
        std::unique_ptr<AccountManager> account_manager;
        std::unique_ptr<TradingOrchestrator> trading_orchestrator;
    };
    
    static TradingSystemComponents create_trading_system(const Config::SystemConfig& config, Monitoring::SystemMonitor& system_monitor, ConnectivityManager& connectivity_manager);
    
private:
    static Config::MultiApiConfig configure_providers_for_mode(const Config::SystemConfig& config);
    static void validate_configuration(const Config::SystemConfig& config);
    static void validate_required_providers(const Config::MultiApiConfig& api_config, 
                                           const Config::TradingModeConfig& mode_config);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // TRADING_SYSTEM_FACTORY_HPP
