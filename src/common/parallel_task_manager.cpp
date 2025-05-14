#include "common/parallel_task_manager.hpp"
#include "common/logger.hpp"
#include <algorithm>
#include <thread>
#include <condition_variable>

ParallelTaskManager::ParallelTaskManager(size_t maxThreads)
    : maxThreads_(maxThreads)
    , isRunning_(false)
    , activeTaskCount_(0) {
}

ParallelTaskManager::~ParallelTaskManager() {
    stop();
}

bool ParallelTaskManager::initialize() {
    if (isRunning_) {
        Logger::error("Task manager already running");
        return false;
    }

    isRunning_ = true;
    workerThread_ = std::thread(&ParallelTaskManager::workerLoop, this);
    return true;
}

void ParallelTaskManager::stop() {
    if (!isRunning_) {
        return;
    }

    isRunning_ = false;
    taskCondition_.notify_all();
    queueCondition_.notify_all();

    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

void ParallelTaskManager::addTask(Task task) {
    std::lock_guard<std::mutex> lock(taskMutex_);
    taskQueue_.push(std::move(task));
    taskCondition_.notify_one();
}

void ParallelTaskManager::workerLoop() {
    while (isRunning_) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(taskMutex_);
            taskCondition_.wait(lock, [this] { 
                return !isRunning_ || !taskQueue_.empty(); 
            });

            if (!isRunning_ && taskQueue_.empty()) {
                break;
            }

            task = std::move(taskQueue_.front());
            taskQueue_.pop();
        }

        processTask(task);
    }
}

size_t ParallelTaskManager::getQueueSize() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(taskMutex_));
    return taskQueue_.size();
}

bool ParallelTaskManager::isRunning() const {
    return isRunning_;
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
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queueMutex_));
    return taskQueue_.size();
}

void ParallelTaskManager::processTasks() {
    cleanupCompletedTasks();
}

void ParallelTaskManager::processTask(Task& task) {
    try {
        task.callback();
        task.promise.set_value();
    } catch (const std::exception& e) {
        task.promise.set_exception(std::current_exception());
        Logger::error("Task execution failed: " + std::string(e.what()));
    }
}

bool ParallelTaskManager::shouldWorkerContinue() const {
    return isRunning_;
}

void ParallelTaskManager::cleanupCompletedTasks() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    activeTasks_.erase(
        std::remove_if(activeTasks_.begin(), activeTasks_.end(),
            [](const Task& task) {
                return task.processId == 0;
            }),
        activeTasks_.end()
    );
}

std::future<void> ParallelTaskManager::addTask(const std::string& taskId, TaskCallback callback) {
    Task task{taskId, std::move(callback), std::promise<void>(), 0};
    auto future = task.promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        taskQueue_.push(std::move(task));
    }

    queueCondition_.notify_one();
    return future;
}

std::mutex ParallelTaskManager::taskMutex_;
std::condition_variable ParallelTaskManager::taskCondition_;
std::queue<Task> ParallelTaskManager::taskQueue_;
size_t ParallelTaskManager::activeTaskCount_ = 0;
std::mutex ParallelTaskManager::queueMutex_;
std::condition_variable ParallelTaskManager::queueCondition_;
std::vector<Task> ParallelTaskManager::activeTasks_;
std::thread ParallelTaskManager::workerThread_;
bool ParallelTaskManager::isRunning_;

} 