#pragma once
#include "Config.h"

class ArgParser {
public:
    static Config parse(int argc, char* argv[]);
    static void print_help(const char* program_name);
    static bool is_valid_url(const std::string& url);
};