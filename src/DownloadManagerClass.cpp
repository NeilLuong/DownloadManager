#include "DownloadManagerClass.h"
#include "Logger.h"

DownloadManager::DownloadManager(size_t maxConcurrent)
    : pool_(maxConcurrent)
    , activeCount_(0)
    , maxConcurrent_(maxConcurrent)
    , running_(false)
    , completedCount_(0)
{
    LOG_INFO("Created DownloadManager with max " + std::to_string(maxConcurrent) + " concurrent downloads");
}

DownloadManager::~DownloadManager() {
    LOG_INFO("Destroying DownloadManager");
    waitForCompletion();
}

void DownloadManager::addDownload(const std::string& url, const std::string& destination, int retryCount, int timeoutSeconds, const std::string& checksum) {
    auto task = std::make_shared<DownloadTask>(url, destination, retryCount, timeoutSeconds, checksum);

    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        tasks_.push_back(task);
    }

    LOG_INFO("Added download: " + url + " -> " + destination);
}

void DownloadManager::start() {
    running_.store(true);
    LOG_INFO("Starting DownloadManager");

    //Launch initial batch of downloads (up to maxConcurrent_)
    size_t tasksToStart = 0;
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        tasksToStart = std::min(tasks_.size(), maxConcurrent_);
    }

    // Start tasks outside the lock to avoid deadlock
    for(size_t i=0; i < tasksToStart; ++i) {
        processNextTask();
    }
}

void DownloadManager::processNextTask() {
    //Find next queued task
    std::shared_ptr<DownloadTask> task;

    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        for (auto& t : tasks_) {
            if (t->getState() == DownloadState::Queued) {
                task = t;
                break;
            }
        }
    }

    if (!task) {
        return; //No queued tasks
    }

    //Check if we can start another download
    if (activeCount_.load() >= maxConcurrent_) {
        return; //Already at max concurrent
    }

    //Increment active count
    activeCount_.fetch_add(1);

    //Submit to thread pool
    pool_.enqueue([this, task] {
        downloadTask(task);
    });
}

void DownloadManager::downloadTask(std::shared_ptr<DownloadTask> task) {
    LOG_INFO("Starting download worker for: " + task->getUrl());

    //Mark task as started
    task->start();

    //Create HTTP client
    CurlHttpClient httpClient;

    //Convert task to config
    Config config = task->toConfig();

    //Perform the download
    bool success = httpClient.download_and_verify(config);

    //Update task state
    if (success) {
        task->markCompleted();
    } else{
        task->markFailed("Download failed");
    }

    //Decrement active count
    activeCount_.fetch_sub(1);
    completedCount_.fetch_add(1);

    LOG_INFO("Downloadworker finished: " + task->getUrl() + " (success: " + (success ? "true" : "false") + ")");

    //Trye to start next queued task
    if (running_.load()) {
        processNextTask();
    }

    //Notify waiters
    workAvailable_.notify_all();
}

void DownloadManager::waitForCompletion() {
    LOG_INFO("Waiting for all downloads to complete...");

    std::unique_lock<std::mutex> lock(taskMutex_);

    //Wait until all tasks are done (not queued or downloading)
    workAvailable_.wait(lock, [this] {
        for (const auto& task : tasks_) {
            DownloadState state = task->getState();
            if (state == DownloadState::Queued || state == DownloadState::Downloading) {
                return false; //Still work to do
            }
        }

        return true; // All done
    });
    running_.store(false);
    LOG_INFO("All downloads complete");
}

size_t DownloadManager::getActiveCount() const {
    return activeCount_.load();
}

size_t DownloadManager::getQueuedCount() const {
    std::lock_guard<std::mutex> lock(taskMutex_);

    size_t count = 0;
    for (const auto& task : tasks_) {
        if (task->getState() == DownloadState::Queued) {
            ++count;
        }
    }

    return count;
}

size_t DownloadManager::getCompletedCount() const {
    return completedCount_.load();
}

size_t DownloadManager::getTotalCount() const {
    std::lock_guard<std::mutex> lock(taskMutex_);
    return tasks_.size();
}

std::shared_ptr<DownloadTask> DownloadManager::getTask(size_t index) const {
    std::lock_guard<std::mutex> lock(taskMutex_);

    if (index >= tasks_.size()) {
        return nullptr;
    }

    return tasks_[index];
}