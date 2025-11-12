#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "DownloadTask.h"
#include "ThreadPool.h"
#include "HttpClient.h"

class DownloadManager {
public:
    explicit DownloadManager(size_t maxConcurrent = 4);

    ~DownloadManager();

    //Add a download to the queue
    void addDownload(const std::string& url, const std::string& destination, int retryCount = 3, int timeoutSeconds = 300, const std::string& checksum = "");

    //Start processing the download queue
    void start();

    //Wait for all downloads to complete
    void waitForCompletion();

    //Get status
    size_t getActiveCount() const;
    size_t getQueuedCount() const;
    size_t getCompletedCount() const;
    size_t getTotalCount() const;

    std::shared_ptr<DownloadTask> getTask(size_t index) const;

private:
//Process next task from queue
    void processNextTask();

    //Worker function that downloads a task
    void downloadTask(std::shared_ptr<DownloadTask> task);

    //Thread pool for concurrent downloads
    ThreadPool pool_;

    //All tasks (queued, active, completed)
    std::vector<std::shared_ptr<DownloadTask>> tasks_;
    mutable std::mutex taskMutex_;

    //Active download tracking
    std::atomic<size_t> activeCount_;
    size_t maxConcurrent_;

    //Synchronization
    std::condition_variable workAvailable_;
    std::atomic<bool> running_;

    //Counters
    std::atomic<size_t> completedCount_;
    
};