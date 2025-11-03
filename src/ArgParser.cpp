#include "ArgParser.h"
#include <iostream>
#include <string>

void ArgParser::print_help(const char* program_name) {
    std::cout << "Download Manager v1.0\n\n";
    std::cout << "USAGE:\n";
    std::cout << "  " << program_name << " <URL [OPTIONS]\n";
    std::cout << "  " << program_name << " --help\n\n";
    
    std::cout << "ARGUEMENTS:\n";
    std::cout << "  <URL>                   URL to download\n\n";

    std::cout << "OPTIONS:\n";
    std::cout << "  -o, --output <file>     Output file path (default: filename from URL)\n";
    std::cout << "  -r, --retry-count <num> Number of retries on failure (default: 3)\n";
    std::cout << "  -t, --timeout <seconds> Download timeout in seconds (default: 300)\n";
    std::cout << "  -c, --connect-timeout <s>  Connection timeout in seconds (default: 30)\n";
    std::cout << "  -h, --help                 Show this help message\n\n";

    std::cout << "EXAMPLES:\n";
    std::cout << "  " << program_name << " http://example.com/file.zip\n";
    std::cout << "  " << program_name << " http://example.com/file.zip -o myfile.zip\n";
    std::cout << "  " << program_name << " http://example.com/file.zip --retry-count 5\n";
    std::cout << "  " << program_name << " http://example.com/file.zip -o output.zip -r 5 -t 600\n";
}

bool ArgParser::is_valid_url(const std::string& url) {
    if (url.empty()) {
        return false;
    }

    if (url.find("http://") == 0 || url.find("https://") == 0) {
        if (url.length() > 8) {
            return true;
        }
    }

    return false;
}

Config ArgParser::parse(int argc, char* argv[]) {
    Config config;

    if (argc < 2) {
        print_help(argv[0]);
        std::exit(1);
    }

    //check for help flag first
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            config.show_help = true;
            return config;
        }
    }

    bool found_url = false;

    //First non-flag arguement is the URL
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        //handle flags with values
        if (arg == "--output" || arg == "-o") {
            if (i + 1 < argc) {
                config.output_path = argv[i+1];
                i++; //skip next arguement
            } else {
                std::cerr << "Error: --output requires a value\n";
                std::exit(1);
            }
        } else if (arg == "--retry-count" || arg == "-r") {
            if (i + 1 < argc) {
                try
                {
                    config.retry_count = std::stoi(argv[i + 1]);
                    if (config.retry_count < 0) {
                        std::cerr << "Error: retry count must be non-negative\n";
                        std::exit(1);
                    }
                    i++;
                }
                catch(const std::exception& e)
                {
                    std::cerr << "Error: invalid try count value\n";
                    std::exit(1);
                }
            } else {
                std::cerr << "Error: --retry-count requires a value\n";
                std::exit(1);
            }
        } else if (arg == "--timeout" || arg == "-t") {
            if (i + 1 < argc) {
                try
                {
                    config.timeout_seconds = std::stoi(argv[i + 1]);
                    if (config.timeout_seconds < 0) {
                        std::cerr << "Error timeout must be positive\n";
                        std::exit(1);
                    }
                    i++;
                }
                catch(const std::exception& e)
                {
                    std::cerr << "Error: invalid timeout value\n";
                    std::exit(1);
                }  
            } else {
                std::cerr << "Error: --timeout requires a value\n";
                std::exit(1);
            }
        }
        else if (arg == "--connect-timeout" || arg == "-c") {
            if (i + 1 < argc) {
                try {
                    config.connect_timeout_seconds = std::stoi(argv[i + 1]);
                    if (config.connect_timeout_seconds <= 0) {
                        std::cerr << "Error: connect-timeout must be positive\n";
                        std::exit(1);
                    }
                    i++;
                } catch (const std::exception& e) {
                    std::cerr << "Error: invalid connect-timeout value\n";
                    std::exit(1);
                }
            } else {
                std::cerr << "Error: --connect-timeout requires a value\n";
                std::exit(1);
            }
        }
        else if (arg[0] == '-') {
            // Unknown flag
            std::cerr << "Error: unknown option '" << arg << "'\n";
            std::cerr << "Use --help for usage information\n";
            std::exit(1);
        }
        else {
            // Positional argument (URL)
            if (!found_url) {
                config.url = arg;
                found_url = true;
            } else {
                // If we already have URL and no --output flag, treat as output path
                if (config.output_path.empty()) {
                    config.output_path = arg;
                }
            }
        }
    }
    if (config.url.empty()) {
        std::cerr << "Error: URL is required\n";
        print_help(argv[0]);
        std::exit(1);
    }

    if (!is_valid_url(config.url)) {
        std::cerr << "Error: invalid URL format '" << config.url << "'\n";
        std::cerr << "URL must start with http:// or https://\n";
        std::exit(1);
    }

    // If no output path specified, extract filename from URL
    if (config.output_path.empty()) {
        // Find last '/' in URL
        size_t last_slash = config.url.find_last_of('/');
        if (last_slash != std::string::npos && last_slash < config.url.length() - 1) {
            config.output_path = config.url.substr(last_slash + 1);
        } else {
            config.output_path = "download.bin";  // Default filename
        }
    }

    return config;
}