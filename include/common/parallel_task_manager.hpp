#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <future>
#include "common/logger.hpp"

namespace vmware {

class ParallelTaskManager {
public:
    using TaskCallback = std::function<void()>;

    ParallelTaskManager(size_t maxConcurrentTasks = 4);
    ~ParallelTaskManager();

    // Add a task to the queue
    std::future<void> addTask(const std::string& taskId, TaskCallback callback);

    // Wait for all tasks to complete
    void waitForAll();

    // Get the number of active tasks
    size_t getActiveTaskCount() const;

    // Get the number of queued tasks
    size_t getQueuedTaskCount() const;

    // Stop all tasks
    void stop();

private:
    struct Task {
        std::string id;
        TaskCallback callback;
        std::promise<void> promise;
    };

    void workerThread();
    void processTask(Task& task);

    std::queue<Task> taskQueue_;
    std::vector<std::thread> workerThreads_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::atomic<bool> running_;
    std::atomic<size_t> activeTaskCount_;
    size_t maxConcurrentTasks_;
};

} // namespace vmware 