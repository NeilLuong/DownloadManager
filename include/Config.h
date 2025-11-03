#pragma once
#include <string>

struct Config {
    std::string url;
    std::string output_path;
    int retry_count;
    int timeout_seconds;
    int connect_timeout_seconds;
    bool show_help;

    Config()
        : url("")
        , output_path("")
        , retry_count(3)
        , timeout_seconds(300)
        , connect_timeout_seconds(30)
        , show_help(false)
        {}
};