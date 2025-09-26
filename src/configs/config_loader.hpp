#ifndef CONFIG_LOADER_HPP
#define CONFIG_LOADER_HPP

#include <string>

struct SystemConfig;

// Load key,value CSV into SystemConfig. Unknown keys are ignored. Returns true on success.
bool load_config_from_csv(SystemConfig& cfg, const std::string& csv_path);

// Load strategy profiles from separate CSV file. Returns true on success.
bool load_strategy_profiles(SystemConfig& cfg, const std::string& strategy_profiles_path);

// Load thread configurations from separate CSV file. Returns true on success.
bool load_thread_configs(SystemConfig& cfg, const std::string& thread_config_path);

// Load complete system configuration (both runtime config and strategy profiles). Returns 0 on success, 1 on failure.
int load_system_config(SystemConfig& config);

// Validate system configuration. Returns true if valid, false otherwise with error message.
bool validate_config(const SystemConfig& config, std::string& errorMessage);

#endif // CONFIG_LOADER_HPP


