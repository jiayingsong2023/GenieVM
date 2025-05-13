#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <queue>
#include <functional>
#include <atomic>
#include <future>
#include <sys/types.h>
#include <unistd.h>
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

    // Process pending tasks - should be called periodically from main thread
    void processTasks();

private:
    struct Task {
        std::string id;
        TaskCallback callback;
        std::promise<void> promise;
        pid_t processId;
    };

    void processTask(Task& task);
    bool shouldWorkerContinue() const;
    void cleanupCompletedTasks();

    std::queue<Task> taskQueue_;
    std::vector<Task> activeTasks_;
    std::mutex queueMutex_;
    std::atomic<bool> running_;
    std::atomic<size_t> activeTaskCount_;
    size_t maxConcurrentTasks_;
};

} // namespace vmware 