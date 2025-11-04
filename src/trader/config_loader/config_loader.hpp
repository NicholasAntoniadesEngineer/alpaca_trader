#ifndef CONFIG_LOADER_HPP
#define CONFIG_LOADER_HPP

#include <string>
#include "configs/system_config.hpp"

bool load_config_from_csv(AlpacaTrader::Config::SystemConfig& cfg, const std::string& csv_path);
bool load_thread_configs(AlpacaTrader::Config::SystemConfig& cfg, const std::string& thread_config_path);
int load_system_config(AlpacaTrader::Config::SystemConfig& config);
bool validate_config(const AlpacaTrader::Config::SystemConfig& config, std::string& errorMessage);

#endif // CONFIG_LOADER_HPP


