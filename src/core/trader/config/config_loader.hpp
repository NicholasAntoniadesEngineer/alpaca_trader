#ifndef CONFIG_LOADER_HPP
#define CONFIG_LOADER_HPP

#include <string>
#include "configs/system_config.hpp"

// Load key,value CSV into SystemConfig. Unknown keys are ignored. Returns true on success.
bool load_config_from_csv(AlpacaTrader::Config::SystemConfig& cfg, const std::string& csv_path);

// Load strategy profiles from separate CSV file. Returns true on success.
bool load_strategy_profiles(AlpacaTrader::Config::SystemConfig& cfg, const std::string& strategy_profiles_path);

// Load thread configurations from separate CSV file. Returns true on success.
bool load_thread_configs(AlpacaTrader::Config::SystemConfig& cfg, const std::string& thread_config_path);

// Load complete system configuration (both runtime config and strategy profiles). Returns 0 on success, 1 on failure.
int load_system_config(AlpacaTrader::Config::SystemConfig& config);

// Validate system configuration. Returns true if valid, false otherwise with error message.
bool validate_config(const AlpacaTrader::Config::SystemConfig& config, std::string& errorMessage);

#endif // CONFIG_LOADER_HPP


