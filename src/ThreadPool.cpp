#include "ThreadPool.h"
#include "Logger.h"

ThreadPool::ThreadPool(size_t numThreads) : stop_(false) {
    LOG_INFO("Creating ThreadPool with " + std::to_string(numThreads) + " threads");
    
    // Create worker threads
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this, i] {
            // Worker loop
            LOG_DEBUG("Worker thread " + std::to_string(i) + " started");
            
            while (true) {
                std::function<void()> task;
                
                {
                    // Wait for task or stop signal
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    
                    // Wait until: (task available) OR (stop flag set)
                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });
                    
                    // Exit if stopping and no more tasks
                    if (stop_ && tasks_.empty()) {
                        LOG_DEBUG("Worker thread " + std::to_string(i) + " exiting");
                        return;
                    }
                    
                    // Get task from queue
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                
                // Execute task (outside the lock!)
                try {
                    task();
                } catch (const std::exception& e) {
                    LOG_ERROR("Task threw exception: " + std::string(e.what()));
                } catch (...) {
                    LOG_ERROR("Task threw unknown exception");
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    LOG_INFO("Shutting down ThreadPool");
    
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    
    // Wake all threads
    condition_.notify_all();
    
    // Wait for all threads to finish
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    LOG_INFO("ThreadPool shutdown complete");
}