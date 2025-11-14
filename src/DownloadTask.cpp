#include "DownloadTask.h"
#include "Logger.h"

std::string stateToString(DownloadState state) {
    switch (state) {
        case DownloadState::Queued:
            return "Queued";
        case DownloadState::Downloading:
            return "Downloading";
        case DownloadState::Paused:
            return "Paused";
        case DownloadState::Completed:
            return "Completed";
        case DownloadState::Failed:
            return "Failed";
        case DownloadState::Canceled:
            return "Canceled";
        default:
            return "Unknown";
    }
}

DownloadTask::DownloadTask(const std::string& url, const std::string& destination, int retryCount, int timeoutSeconds, const std::string& checksum) 
    : url_(url)
    , destination_(destination)
    , state_(DownloadState::Queued)
    , retryCount_(retryCount)
    , timeoutSeconds_(timeoutSeconds)
    , connectTimeoutSeconds_(30)
    , expectedChecksum_(checksum)
    , bytesDownloaded_(0)
    , totalBytes_(0)
{
    LOG_INFO("Created download task: " + url + " -> " + destination);
}

void DownloadTask::start() {
    DownloadState expected = DownloadState::Queued;
    if (state_.compare_exchange_strong(expected, DownloadState::Downloading)) {
        startTime_ = std::chrono::steady_clock::now();
        LOG_INFO("Download started: " + url_);
    } else {
        LOG_WARN("Cannot start download, current state: " + stateToString(expected));
    }
}

// void DownloadTask::pause() {
//     DownloadState expected = DownloadState::Downloading;
//     if (state_.compare_exchange_strong(expected, DownloadState::Paused)) {
//         LOG_INFO("Download paused: " + url_);
//     } else {
//         LOG_WARN("Cannot pause download, current state: " + stateToString(expected));
//     }
// }

void DownloadTask::resume() {
    DownloadState expected = DownloadState::Paused;
    if (state_.compare_exchange_strong(expected, DownloadState::Downloading)) {
        LOG_INFO("Download resumed: " + url_);
    } else {
        LOG_WARN("Cannot resume download, current state: " + stateToString(expected));
    }
}

void DownloadTask::cancel() {
    DownloadState expected = state_.load();
    while (expected != DownloadState::Completed && expected != DownloadState::Failed && expected != DownloadState::Canceled) {
        if (state_.compare_exchange_strong(expected, DownloadState::Canceled)) {
            LOG_INFO("Download canceled: " + url_);
            return;
        }
    }
    LOG_WARN("Cannot cancel download, current state: " + stateToString(expected));
}

void DownloadTask::markCompleted() {
    state_.store(DownloadState::Completed);
    LOG_INFO("Download completed: " + url_);
}

void DownloadTask::markFailed(const std::string& errorMessage) {
    {
        std::lock_guard<std::mutex> lock(errorMutex_);
        errorMessage_ = errorMessage;
    }
    state_.store(DownloadState::Failed);
    LOG_ERROR("Download failed: " + url_ + " Error: " + errorMessage);
}

DownloadState DownloadTask::getState() const {
    return state_.load(std::memory_order_relaxed);
}

std::string DownloadTask::getErrorMessage() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return errorMessage_;
}

void DownloadTask::updateProgress(size_t bytesDownloaded, size_t totalBytes) {
    bytesDownloaded_.store(bytesDownloaded, std::memory_order_relaxed);
    totalBytes_.store(totalBytes, std::memory_order_relaxed);
}

size_t DownloadTask::getBytesDownloaded() const {
    return bytesDownloaded_.load(std::memory_order_relaxed);
}

size_t DownloadTask::getTotalBytes() const {
    return totalBytes_.load(std::memory_order_relaxed);
}

double DownloadTask::getProgressPercentage() const {
    size_t total = totalBytes_.load(std::memory_order_relaxed);
    size_t downloaded = bytesDownloaded_.load(std::memory_order_relaxed);

    if (total == 0) {
        return 0.0;
    }

    return (static_cast<double>(downloaded) / total) * 100.0;
}

Config DownloadTask::toConfig() const {
    Config config;
    config.url = url_;
    config.output_path = destination_;

    config.retry_count = retryCount_;
    config.timeout_seconds = timeoutSeconds_;
    config.connect_timeout_seconds = connectTimeoutSeconds_;
    config.expected_checksum = expectedChecksum_;
    config.verify_checksum = !expectedChecksum_.empty();

    config.show_help = false;
    config.default_download_dir = ".";
    return config;
}

bool DownloadTask::shouldContinue() const {
    DownloadState state = state_.load(std::memory_order_relaxed);
    return (state == DownloadState::Downloading);
}

bool DownloadTask::waitForPause(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(pauseMutex_);

    //Wait until state is Paused (or timeout)
    bool paused = pauseConfirmed_.wait_for(lock, timeout, [this] {
        return state_.load() == DownloadState::Paused;
    });

    if (!paused) {
        LOG_WARN("Pause timeout for: " + url_);
    }

    return paused;
}

void DownloadTask::pause() {
    DownloadState expected = DownloadState::Downloading;

    if (state_.compare_exchange_strong(expected, DownloadState::Paused)) {
        LOG_INFO("Download paused: " + url_);
        pauseConfirmed_.notify_all();
    } else {
        LOG_WARN("Cannot pause download, current state: " + stateToString(expected));
    }
}

