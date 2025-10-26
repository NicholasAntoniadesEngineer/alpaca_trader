#include "trading_system_factory.hpp"
#include "core/logging/async_logger.hpp"
#include "core/threads/thread_register.hpp"
#include <stdexcept>

namespace AlpacaTrader {
namespace Core {

TradingSystemFactory::TradingSystemComponents TradingSystemFactory::create_trading_system(const Config::SystemConfig& config, Monitoring::SystemMonitor& system_monitor, ConnectivityManager& connectivity_manager) {
    validate_configuration(config);
    
    TradingSystemComponents components;
    
    // Configure API providers based on trading mode
    Config::MultiApiConfig filtered_api_config = configure_providers_for_mode(config);
    
    // Create API manager
    components.api_manager = std::make_unique<API::ApiManager>(filtered_api_config);
    
    // Create account manager
    AlpacaTrader::Config::AccountManagerConfig account_config{
        config.logging, config.timing, config.strategy
    };
    components.account_manager = std::make_unique<AccountManager>(account_config, *components.api_manager);
    
    // Create trading orchestrator
    components.trading_orchestrator = std::make_unique<TradingOrchestrator>(
        config, *components.api_manager, *components.account_manager, system_monitor, connectivity_manager);
    
    return components;
}

Config::MultiApiConfig TradingSystemFactory::configure_providers_for_mode(const Config::SystemConfig& config) {
    Config::MultiApiConfig filtered_config;
    
    if (config.trading_mode.is_stocks()) {
        // For stocks mode, use Alpaca trading and stocks providers
        if (config.multi_api.has_provider(Config::ApiProvider::ALPACA_TRADING)) {
            filtered_config.providers[Config::ApiProvider::ALPACA_TRADING] = 
                config.multi_api.get_provider_config(Config::ApiProvider::ALPACA_TRADING);
        }
        
        if (config.multi_api.has_provider(Config::ApiProvider::ALPACA_STOCKS)) {
            filtered_config.providers[Config::ApiProvider::ALPACA_STOCKS] = 
                config.multi_api.get_provider_config(Config::ApiProvider::ALPACA_STOCKS);
        }
        
        AlpacaTrader::Logging::log_message("Configured for stocks trading mode", "");
        
    } else if (config.trading_mode.is_crypto()) {
        // For crypto mode, use Alpaca trading and Polygon crypto providers
        if (config.multi_api.has_provider(Config::ApiProvider::ALPACA_TRADING)) {
            filtered_config.providers[Config::ApiProvider::ALPACA_TRADING] = 
                config.multi_api.get_provider_config(Config::ApiProvider::ALPACA_TRADING);
        }
        
        if (config.multi_api.has_provider(Config::ApiProvider::POLYGON_CRYPTO)) {
            filtered_config.providers[Config::ApiProvider::POLYGON_CRYPTO] = 
                config.multi_api.get_provider_config(Config::ApiProvider::POLYGON_CRYPTO);
        }
        
        AlpacaTrader::Logging::log_message("Configured for crypto trading mode", "");
        
    } else {
        throw std::runtime_error("Unknown trading mode configuration");
    }
    
    validate_required_providers(filtered_config, config.trading_mode);
    
    return filtered_config;
}

void TradingSystemFactory::validate_configuration(const Config::SystemConfig& config) {
    if (config.trading_mode.primary_symbol.empty()) {
        throw std::runtime_error("Primary symbol is required but not configured");
    }
    
    if (config.multi_api.providers.empty()) {
        throw std::runtime_error("No API providers configured");
    }
    
    // Validate that we have the minimum required configuration
    if (!config.multi_api.has_provider(Config::ApiProvider::ALPACA_TRADING)) {
        throw std::runtime_error("Alpaca trading provider is required but not configured");
    }
}

void TradingSystemFactory::validate_required_providers(const Config::MultiApiConfig& api_config, 
                                                      const Config::TradingModeConfig& mode_config) {
    if (mode_config.is_stocks()) {
        if (!api_config.has_provider(Config::ApiProvider::ALPACA_TRADING)) {
            throw std::runtime_error("Alpaca trading provider is required for stocks mode");
        }
        
        if (!api_config.has_provider(Config::ApiProvider::ALPACA_STOCKS)) {
            AlpacaTrader::Logging::log_message("Warning: Alpaca stocks provider not configured, using trading provider for market data", "");
        }
        
    } else if (mode_config.is_crypto()) {
        if (!api_config.has_provider(Config::ApiProvider::ALPACA_TRADING)) {
            throw std::runtime_error("Alpaca trading provider is required for crypto mode");
        }
        
        if (!api_config.has_provider(Config::ApiProvider::POLYGON_CRYPTO)) {
            throw std::runtime_error("Polygon crypto provider is required for crypto mode but not configured");
        }
    }
}

} // namespace Core
} // namespace AlpacaTrader
