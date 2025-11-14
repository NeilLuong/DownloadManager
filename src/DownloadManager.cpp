#include <iostream>
#include <curl/curl.h>
#include <string>
#include <cassert>
#include "HttpClient.h"
#include "ArgParser.h"
#include "Config.h"
#include "Logger.h"
#include "ThreadPool.h"
#include "DownloadTask.h"
#include <chrono>
#include <DownloadManagerClass.h>

void test_download_manager();
void test_thread_pool();
void test_download_task();
void test_pause_resume();

int main(int argc, char* argv[]) {
    //TestThreadPool
    if (argc == 2 && std::string(argv[1]) == "--test-threadpool") {
        test_thread_pool();
        return 0;
    }

    //TestDownloadTask
    if (argc == 2 && std::string(argv[1]) == "--test-downloadtask") {
        test_download_task();
        return 0;
    }

    if (argc == 2 && std::string(argv[1]) == "--test-downloadmanager") {
        test_download_manager();
        return 0;
    }

    if (argc == 2 && std::string(argv[1]) == "--test-pauseresume") {
    test_pause_resume();
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

    // Test 3: Void return type
    std::cout << "\nTest 3: Void return type...\n";
    int counter = 0;
    std::vector<std::future<void>> voidFutures;

    for (int i = 0; i < 5; ++i) {
        voidFutures.emplace_back(
            pool.enqueue([i, &counter] {
                std::cout << "  Void task " << i << " executing\n";
                counter++;  // Side effect instead of return value
            })
        );
    }

    // Wait for all void tasks to complete
    for (auto& f : voidFutures) {
        f.get();  // get() on void future just blocks until complete
    }

    std::cout << "  All void tasks completed. Counter = " << counter << "\n";

    std::cout << "\n=== ThreadPool tests complete ===\n\n";
}

void test_download_task() {
    std::cout << "\n=== Testing DownloadTask ===\n\n";

    // Test 1: State transitions
    std::cout << "Test 1: State transitions...\n";
    DownloadTask task("http://example.com/file.zip", "output.zip", 3, 300, "");

    std::cout << "  Initial state: " << stateToString(task.getState()) << "\n";
    assert(task.getState() == DownloadState::Queued);

    task.start();
    std::cout << "  After start(): " << stateToString(task.getState()) << "\n";
    assert(task.getState() == DownloadState::Downloading);

    task.pause();
    std::cout << "  After pause(): " << stateToString(task.getState()) << "\n";
    assert(task.getState() == DownloadState::Paused);

    task.resume();
    std::cout << "  After resume(): " << stateToString(task.getState()) << "\n";
    assert(task.getState() == DownloadState::Downloading);

    task.markCompleted();
    std::cout << "  After markCompleted(): " << stateToString(task.getState()) << "\n";
    assert(task.getState() == DownloadState::Completed);

    // Test 2: Invalid transitions
    std::cout << "\nTest 2: Invalid transitions...\n";
    task.start();  // Should fail - already completed
    assert(task.getState() == DownloadState::Completed);
    std::cout << "  Cannot start completed task: OK\n";

    // Test 3: Progress tracking
    std::cout << "\nTest 3: Progress tracking...\n";
    DownloadTask task2("http://example.com/big.zip", "big.zip", 3, 300, "");
    task2.start();

    task2.updateProgress(0, 1000);
    std::cout << "  Progress: " << task2.getProgressPercentage() << "%\n";
    assert(task2.getProgressPercentage() == 0.0);

    task2.updateProgress(500, 1000);
    std::cout << "  Progress: " << task2.getProgressPercentage() << "%\n";
    assert(task2.getProgressPercentage() == 50.0);

    task2.updateProgress(1000, 1000);
    std::cout << "  Progress: " << task2.getProgressPercentage() << "%\n";
    assert(task2.getProgressPercentage() == 100.0);

    // Test 4: Error handling
    std::cout << "\nTest 4: Error handling...\n";
    DownloadTask task3("http://example.com/fail.zip", "fail.zip", 3, 300, "");
    task3.start();
    task3.markFailed("Connection timeout");

    assert(task3.getState() == DownloadState::Failed);
    std::cout << "  Error message: " << task3.getErrorMessage() << "\n";
    assert(task3.getErrorMessage() == "Connection timeout");

    // Test 5: Config integration
    std::cout << "\nTest 5: Config integration...\n";
    DownloadTask task4("http://example.com/test.zip", "test.zip", 5, 600, "abc123def");
    Config config = task4.toConfig();

    assert(config.url == "http://example.com/test.zip");
    assert(config.output_path == "test.zip");
    assert(config.retry_count == 5);
    assert(config.timeout_seconds == 600);
    assert(config.expected_checksum == "abc123def");
    assert(config.verify_checksum == true);
    std::cout << "  Config values correct\n";

    // Test 6: Thread safety (concurrent reads)
    std::cout << "\nTest 6: Thread safety...\n";
    DownloadTask task5("http://example.com/concurrent.zip", "concurrent.zip", 3, 300, "");
    task5.start();
    task5.updateProgress(0, 1000000);

    ThreadPool pool(4);
    std::vector<std::future<void>> futures;

    // Simulate multiple threads reading state/progress
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.enqueue([&task5, i] {
            // Simulate progress updates from download thread
            if (i % 2 == 0) {
                task5.updateProgress(i * 10000, 1000000);
            }

            // Simulate UI thread reading progress
            auto state = task5.getState();
            auto progress = task5.getProgressPercentage();
            auto bytes = task5.getBytesDownloaded();

            // Just access the values (proves no data races)
            (void)state;
            (void)progress;
            (void)bytes;
        }));
    }

    // Wait for all
    for (auto& f : futures) {
        f.get();
    }

    std::cout << "  100 concurrent operations completed without crashes\n";

    std::cout << "\n=== DownloadTask tests complete ===\n\n";
}

void test_download_manager() {
    std::cout << "\n=== Testing DownloadManager ===\n\n";
    
    // Test 1: Download 5 files with max 2 concurrent
    std::cout << "Test 1: Download 5 small files (max 2 concurrent)...\n";
    
    DownloadManager manager(2);  // Max 2 concurrent
    
    // Add 5 downloads
    manager.addDownload("https://httpbin.org/bytes/100", "test1.bin", 3, 30, "");
    manager.addDownload("https://httpbin.org/bytes/200", "test2.bin", 3, 30, "");
    manager.addDownload("https://httpbin.org/bytes/300", "test3.bin", 3, 30, "");
    manager.addDownload("https://httpbin.org/bytes/400", "test4.bin", 3, 30, "");
    manager.addDownload("https://httpbin.org/bytes/500", "test5.bin", 3, 30, "");
    
    std::cout << "  Total tasks: " << manager.getTotalCount() << "\n";
    std::cout << "  Queued: " << manager.getQueuedCount() << "\n";
    
    // Start downloads
    manager.start();
    
    std::cout << "  Started! Active: " << manager.getActiveCount() << "\n";
    
    // Monitor progress
    while (manager.getCompletedCount() < manager.getTotalCount()) {
        std::cout << "  Status: Active=" << manager.getActiveCount()
                  << " Queued=" << manager.getQueuedCount()
                  << " Completed=" << manager.getCompletedCount()
                  << "/" << manager.getTotalCount() << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Wait for completion
    manager.waitForCompletion();
    
    std::cout << "  All downloads complete!\n";
    std::cout << "  Final: Completed=" << manager.getCompletedCount()
              << " Total=" << manager.getTotalCount() << "\n";
    
    // Verify files exist
    std::cout << "\nTest 2: Verify downloaded files...\n";
    for (size_t i = 1; i <= 5; ++i) {
        std::string filename = "test" + std::to_string(i) + ".bin";
        if (std::filesystem::exists(filename)) {
            size_t size = std::filesystem::file_size(filename);
            std::cout << "  " << filename << ": " << size << " bytes ✓\n";
        } else {
            std::cout << "  " << filename << ": MISSING ✗\n";
        }
    }
    
    std::cout << "\n=== DownloadManager tests complete ===\n\n";
}

void test_pause_resume() {
    std::cout << "\n=== Testing Pause/Resume ===\n\n";
    
    std::cout << "Test 1: Pause and resume a single download...\n";
    
    DownloadManager manager(2);
    
    // Add a large download (so we have time to pause it)
    manager.addDownload("https://httpbin.org/bytes/1000000", "large.bin", 3, 300, "");
    
    manager.start();
    
    std::cout << "  Download started...\n";
    
    // Let it download for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "  Pausing download...\n";
    manager.pauseDownload("https://httpbin.org/bytes/1000000");
    
    std::cout << "  Download paused. Waiting 2 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "  Resuming download...\n";
    manager.resumeDownload("https://httpbin.org/bytes/1000000");
    
    manager.waitForCompletion();
    
    std::cout << "  Download completed!\n";
    
    // Verify file
    if (std::filesystem::exists("large.bin")) {
        size_t size = std::filesystem::file_size("large.bin");
        std::cout << "  File size: " << size << " bytes\n";
        
        if (size == 1000000) {
            std::cout << "  ✓ File complete and correct size\n";
        } else {
            std::cout << "  ✗ File size mismatch\n";
        }
    }
    
    std::cout << "\n=== Pause/Resume tests complete ===\n\n";
}

