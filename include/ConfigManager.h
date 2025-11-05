#pragma once

#include "Config.h"
#include <filesystem>
#include <string>

class ConfigManager {
public:
    static std::filesystem::path get_config_path();
    static Config load_config();
    static void save_config(const Config& config);
    static void ensure_config_exists();

    //merge config
    static Config merge_configs(const Config& file_config, const Config& cli_config);
private:
};