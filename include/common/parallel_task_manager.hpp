#pragma once

#include <string>
#include <functional>
#include <future>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>

class ParallelTaskManager {
public:
    using TaskCallback = std::function<void()>;

    struct Task {
        std::string id;
        TaskCallback callback;
        std::promise<void> promise;
        int processId;
    };

    explicit ParallelTaskManager(size_t maxThreads = 4);
    ~ParallelTaskManager();

    bool initialize();
    void stop();
    void addTask(Task task);
    std::future<void> addTask(const std::string& taskId, TaskCallback callback);
    size_t getQueueSize() const;
    bool isRunning() const;
    void waitForAll();
    size_t getActiveTaskCount() const;
    size_t getQueuedTaskCount() const;
    void processTasks();

private:
    void workerLoop();
    void processTask(Task& task);
    bool shouldWorkerContinue() const;
    void cleanupCompletedTasks();

    size_t maxThreads_;
    bool isRunning_;
    size_t activeTaskCount_;
    std::mutex taskMutex_;
    std::condition_variable taskCondition_;
    std::queue<Task> taskQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::vector<Task> activeTasks_;
    std::thread workerThread_;
}; 