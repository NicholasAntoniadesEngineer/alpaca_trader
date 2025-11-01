#ifndef MULTI_API_CONFIG_LOADER_HPP
#define MULTI_API_CONFIG_LOADER_HPP

#include "configs/multi_api_config.hpp"
#include <string>

namespace AlpacaTrader {
namespace Core {

class MultiApiConfigLoader {
public:
    static Config::MultiApiConfig load_from_csv(const std::string& csv_path);
    
private:
    static void parse_provider_config(const std::string& key, const std::string& value, 
                                    Config::MultiApiConfig& config);
    static Config::ApiProvider get_provider_from_key(const std::string& key);
    static std::string trim(const std::string& str);
    static bool to_bool(const std::string& str);
    static void validate_required_fields(const Config::MultiApiConfig& config);
    static void validate_provider_config(Config::ApiProvider provider, 
                                       const Config::ApiProviderConfig& config);
};

} // namespace Core
} // namespace AlpacaTrader

#endif // MULTI_API_CONFIG_LOADER_HPP
