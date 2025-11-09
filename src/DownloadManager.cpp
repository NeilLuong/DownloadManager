#include <iostream>
#include <curl/curl.h>
#include <string>
#include "HttpClient.h"
#include "ArgParser.h"
#include "Config.h"
#include "Logger.h"
#include "ThreadPool.h"
#include <iostream>
#include <chrono>

void test_thread_pool();

int main(int argc, char* argv[]) {
    //TestThreadPool
    if (argc == 2 && std::string(argv[1]) == "--test-threadpool") {
        test_thread_pool();
        return 0;
    }
    //TestEnd
    
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

        // Internal logging
    LOG_INFO("Starting download: " + config.url + " -> " + config.output_path);


    std::cout << std::endl;

    bool success = httpClient.download_and_verify(config);
    
    if (success) {
        LOG_INFO("Download completed successfully: " + config.output_path);
    } else {
        LOG_ERROR("Download failed: " + config.url);
    }
    
    return success ? 0 : 1;
}

void test_thread_pool() {
    std::cout << "\n=== Testing ThreadPool ===\n\n";
    
    // Create pool with 4 threads
    ThreadPool pool(4);
    
    // Test 1: Simple tasks
    std::cout << "Test 1: Running 10 simple tasks...\n";
    std::vector<std::future<int>> results;
    
    for (int i = 0; i < 10; ++i) {
        results.emplace_back(
            pool.enqueue([i] {
                std::cout << "  Task " << i << " running on thread " 
                          << std::this_thread::get_id() << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return i * i;  // Return i squared
            })
        );
    }
    
    // Get results
    std::cout << "\nResults:\n";
    for (size_t i = 0; i < results.size(); ++i) {
        int result = results[i].get();  // Blocks until task completes
        std::cout << "  Task " << i << " result: " << result << "\n";
    }
    
    // Test 2: Exception handling
    std::cout << "\nTest 2: Exception handling...\n";
    auto future = pool.enqueue([] {
        throw std::runtime_error("Test exception");
        return 42;
    });
    
    try {
        future.get();
    } catch (const std::exception& e) {
        std::cout << "  Caught exception: " << e.what() << "\n";
    }
    
    std::cout << "\n=== ThreadPool tests complete ===\n\n";
}

