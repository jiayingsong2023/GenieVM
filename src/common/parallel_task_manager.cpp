#include "common/parallel_task_manager.hpp"
#include <algorithm>
#include <stdexcept>

ParallelTaskManager::ParallelTaskManager(size_t numThreads)
    : stop_(false)
    , activeTasks_(0) {
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
    }
    
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back(&ParallelTaskManager::workerThread, this);
    }
}

ParallelTaskManager::~ParallelTaskManager() {
    stop();
    waitForAll();
    
    for (auto& thread : workers_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void ParallelTaskManager::waitForAll() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    condition_.wait(lock, [this] {
        return tasks_.empty() && activeTasks_ == 0;
    });
}

void ParallelTaskManager::stop() {
    stop_ = true;
    condition_.notify_all();
}

void ParallelTaskManager::workerThread() {
    while (!stop_) {
        std::function<void()> taskFunc;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            condition_.wait(lock, [this] {
                return !tasks_.empty() || stop_;
            });
            
            if (stop_ && tasks_.empty()) {
                return;
            }
            
            taskFunc = std::move(tasks_.top().func);
            tasks_.pop();
            ++activeTasks_;
        }
        
        try {
            if (taskFunc) {
                taskFunc();
            }
        } catch (...) {
            // Log error or handle exception
        }
        
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            --activeTasks_;
            if (tasks_.empty() && activeTasks_ == 0) {
                condition_.notify_all();
            }
        }
    }
}

size_t ParallelTaskManager::getActiveThreadCount() const {
    return workers_.size();
}

TaskStats ParallelTaskManager::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
} 