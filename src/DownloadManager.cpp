#include <iostream>
#include <curl/curl.h>
#include <string>
#include "HttpClient.h"
#include "ArgParser.h"
#include "Config.h"

int main(int argc, char* argv[]) {
    Config config = ArgParser::parse(argc, argv);

    if (config.show_help) {
        return 0;
    }

    // Create HTTP client and start download
    CurlHttpClient httpClient;
    
    std::cout << "Downloading: " << config.url << std::endl;
    std::cout << "Output: " << config.output_path << std::endl;
    std::cout << "Retry count: " << config.retry_count << std::endl;
    std::cout << "Timeout: " << config.timeout_seconds << "s" << std::endl;

    if (config.verify_checksum) {
        std::cout << "Checksum verification: enabled" << std::endl;
        std::cout << "Expected hash: " << config.expected_checksum.substr(0, 16) << "..." << std::endl;
    }

    std::cout << std::endl;

    bool success = httpClient.download_and_verify(config);
    
    return success ? 0 : 1;
}

