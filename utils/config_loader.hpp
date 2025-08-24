#ifndef CONFIG_LOADER_HPP
#define CONFIG_LOADER_HPP

#include <string>

struct SystemConfig;

// Load key,value CSV into SystemConfig. Unknown keys are ignored. Returns true on success.
bool load_config_from_csv(SystemConfig& cfg, const std::string& csv_path);

#endif // CONFIG_LOADER_HPP


