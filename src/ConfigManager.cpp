#include "ConfigManager.h"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include "Logger.h"

using json = nlohmann::json;

std::filesystem::path ConfigManager::get_config_path() {
    std::filesystem::path config_dir;

    #ifdef _WIN32
        //Windows: Use APPDATA
        const char* appdata = std::getenv("APPDATA");
        if (appdata) {
            config_dir = std::filesystem::path(appdata) / "DownloadManager";
        } else {
            config_dir = std::filesystem::current_path() / ".config" / "DownloadManager"
        }
    #else 
        //macOS/Linux: Use Home/.config
        const char* home = std::getenv("HOME");
        if (home) {
            config_dir = std::filesystem::path(home) / ".config" / "DownloadManager";
        } else {
            config_dir = std::filesystem::current_path() / ".config" / "DownloadManager";
        }
    #endif

    try {
        if (!std::filesystem::exists(config_dir)) {
            std::filesystem::create_directories(config_dir);
            std::string msg = "Created config directory: " + config_dir.string();
            LOG_INFO(msg);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error creating config directory: " << e.what() << std::endl;
    }

    return config_dir / "config.json";
}

Config ConfigManager::load_config() {
    Config config;
    std::filesystem::path config_path = get_config_path();

    if (!std::filesystem::exists(config_path)) {
        return config;
    }

    try
    {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "Could not open config file: " << config_path << std::endl;
            return config;
        }

        //parse JSON
        json j;
        file >> j;

        //load values from JSON if exists
        if (j.contains("retry_count")) {
            config.retry_count = j["retry_count"];
        }
        if (j.contains("timeout_seconds")) {
            config.timeout_seconds = j["timeout_seconds"];
        }
        if (j.contains("connect_timeout_seconds")) {
            config.connect_timeout_seconds = j["connect_timeout_seconds"];
        }
        if (j.contains("default_download_dir")) {
            config.default_download_dir = j["default_download_dir"].get<std::string>();
        }
        
        std::cout << "Loaded config from: " << config_path << std::endl;

    }
    catch(const json::exception& e)
    {
        std::cerr << "Error parsing config file: " << e.what() << std::endl;
        std::cerr << "Using default configuration" << std::endl;
    }
    return config;
}

void ConfigManager::save_config(const Config& config) {
    std::filesystem::path config_path = get_config_path();

    try
    {
        //Create JSON object
        json j;
        j["retry_count"] = config.retry_count;
        j["timeout_seconds"] = config.timeout_seconds;
        j["connect_timeout_seconds"] = config.connect_timeout_seconds;
        j["default_download_dir"] = config.default_download_dir;

        std::ofstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "Could not write config file: " << config_path << std::endl;
            return;
        }
        
        file << j.dump(4);  // 4 spaces indentation
        
        std::cout << "Saved config to: " << config_path << std::endl;
        
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error saving config: " << e.what() << std::endl;
    }
    
}

void ConfigManager::ensure_config_exists() {
    std::filesystem::path config_path = get_config_path();

    if (!std::filesystem::exists(config_path)) {
        Config default_config;
        save_config(default_config);
        std::cout << "Created default config file" << std::endl;
    }
}

Config ConfigManager::merge_configs(const Config& file_config, const Config& cli_config) {
    Config merged = file_config;

    Config defaults;

    if (cli_config.retry_count != defaults.retry_count) {
        merged.retry_count = cli_config.retry_count;
    }

    if (cli_config.timeout_seconds != defaults.timeout_seconds) {
        merged.timeout_seconds = cli_config.timeout_seconds;
    }

    if (cli_config.connect_timeout_seconds != defaults.connect_timeout_seconds) {
        merged.connect_timeout_seconds = cli_config.connect_timeout_seconds;
    }

    merged.url = cli_config.url;
    merged.output_path = cli_config.output_path;
    merged.show_help = cli_config.show_help;
    merged.verify_checksum = cli_config.verify_checksum;
    merged.expected_checksum = cli_config.expected_checksum;

    //If output_path is empty but default_download_dir is set, use it
    if (merged.output_path.empty() && !merged.default_download_dir.empty()) {
        size_t last_slash = merged.url.find_last_of('/');
        std::string filename = "download.bin";
        if (last_slash != std::string::npos && last_slash < merged.url.length() - 1) {
            filename = merged.url.substr(last_slash + 1);
        }
        merged.output_path = merged.default_download_dir + "/" + filename;
    }
    return merged;
}



