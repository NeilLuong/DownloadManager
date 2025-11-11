#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <chrono>
#include "Config.h"

enum class DownloadState{
    Queued,
    Downloading,
    Paused,
    Completed,
    Failed,
    Canceled
};

class DownloadTask {
public:
    DownloadTask(const std::string& url, const std::string& destination, int retryCount, int timeoutSeconds, const std::string& checksum);

    //State management (must be thread-sage)
    void start();
    void pause();
    void resume();
    void cancel();
    void markCompleted();
    void markFailed(const std::string& errorMessage);

    //Getters (must be thread-safe)
    DownloadState getState() const;
    std::string getUrl() const { return url_; };
    std::string getDesitnation() const { return destination_; };
    std::string getErrorMessage() const;

    int getRetryCount() const { return retryCount_; }
    int getTimeoutSeconds() const { return timeoutSeconds_; }
    int getConnectTimeoutSeconds() const { return connectTimeoutSeconds_; }
    std::string getExpectedChecksum() const { return expectedChecksum_; }
    bool shouldVerifyChecksum() const { return !expectedChecksum_.empty(); }
    

    //Progress tracking (must be thread-safe)
    void updateProgress(size_t bytesDownloaded, size_t totalBytes);
    size_t getBytesDownloaded() const;
    size_t getTotalBytes() const;
    double getProgressPercentage() const;

    //For integration with Config
    Config toConfig() const;
private:
    std::string url_;
    std::string destination_;
    int retryCount_;
    int timeoutSeconds_;
    int connectTimeoutSeconds_;
    std::string expectedChecksum_;

    //State (atomic for lock-free reads)
    std::atomic<DownloadState> state_;

    //Progress (atomics for lock-free reads)
    std::atomic<size_t> bytesDownloaded_;
    std::atomic<size_t> totalBytes_;

    //Error info (needs mutex because string isnt atomic)
    std::string errorMessage_;
    mutable std::mutex errorMutex_;

    //timing 
    std::chrono::steady_clock::time_point startTime_;
};

//Helper function to convert state to string
std::string stateToString(DownloadState state);