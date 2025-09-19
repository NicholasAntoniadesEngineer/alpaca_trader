#ifndef CONFIG_LOADER_HPP
#define CONFIG_LOADER_HPP

#include <string>

struct SystemConfig;

// Load key,value CSV into SystemConfig. Unknown keys are ignored. Returns true on success.
bool load_config_from_csv(SystemConfig& cfg, const std::string& csv_path);

// Load strategy profiles from separate CSV file. Returns true on success.
bool load_strategy_profiles(SystemConfig& cfg, const std::string& strategy_profiles_path);

#endif // CONFIG_LOADER_HPP


