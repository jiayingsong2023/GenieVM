#include "common/parallel_task_manager.hpp"

namespace vmware {

ParallelTaskManager::ParallelTaskManager(size_t maxConcurrentTasks)
    : running_(true)
    , activeTaskCount_(0)
    , maxConcurrentTasks_(maxConcurrentTasks)
{
    // Create worker threads
    for (size_t i = 0; i < maxConcurrentTasks_; ++i) {
        workerThreads_.emplace_back(&ParallelTaskManager::workerThread, this);
    }
}

ParallelTaskManager::~ParallelTaskManager() {
    stop();
}

std::future<void> ParallelTaskManager::addTask(const std::string& taskId, TaskCallback callback) {
    Task task;
    task.id = taskId;
    task.callback = callback;
    task.promise = std::promise<void>();

    std::future<void> future = task.promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        taskQueue_.push(std::move(task));
    }

    queueCondition_.notify_one();
    return future;
}

void ParallelTaskManager::waitForAll() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    queueCondition_.wait(lock, [this] {
        return taskQueue_.empty() && activeTaskCount_ == 0;
    });
}

size_t ParallelTaskManager::getActiveTaskCount() const {
    return activeTaskCount_;
}

size_t ParallelTaskManager::getQueuedTaskCount() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return taskQueue_.size();
}

void ParallelTaskManager::stop() {
    if (!running_) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        running_ = false;
    }

    queueCondition_.notify_all();

    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    workerThreads_.clear();
}

void ParallelTaskManager::workerThread() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        
        queueCondition_.wait(lock, [this] {
            return !running_ || !taskQueue_.empty();
        });

        if (!running_ && taskQueue_.empty()) {
            break;
        }

        Task task = std::move(taskQueue_.front());
        taskQueue_.pop();
        ++activeTaskCount_;

        // Release the lock while processing the task
        lock.unlock();

        try {
            processTask(task);
        } catch (const std::exception& e) {
            Logger::error("Error processing task " + task.id + ": " + std::string(e.what()));
            task.promise.set_exception(std::current_exception());
        }

        --activeTaskCount_;
        queueCondition_.notify_all();
    }
}

void ParallelTaskManager::processTask(Task& task) {
    try {
        task.callback();
        task.promise.set_value();
    } catch (...) {
        task.promise.set_exception(std::current_exception());
        throw;
    }
}

} // namespace vmware 