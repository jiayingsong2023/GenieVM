#pragma once

#include "common/parallel_task_manager.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class Scheduler {
public:
    using TaskCallback = std::function<void()>;
    using TimePoint = std::chrono::system_clock::time_point;
    
    explicit Scheduler(size_t numThreads = std::thread::hardware_concurrency());
    ~Scheduler();

    // Schedule a one-time task
    void scheduleTask(const std::string& taskId, 
                     const TaskCallback& task,
                     const TimePoint& time);

    // Schedule a recurring task
    void scheduleRecurringTask(const std::string& taskId,
                             const TaskCallback& task,
                             std::chrono::seconds interval);

    // Cancel a scheduled task
    void cancelTask(const std::string& taskId);

    // Check if a task is scheduled
    bool isTaskScheduled(const std::string& taskId) const;

    // Get the next execution time for a task
    TimePoint getNextExecutionTime(const std::string& taskId) const;

private:
    void scheduleLoop();
    void executeTask(const std::string& taskId, const TaskCallback& task);

    struct TaskInfo {
        TaskCallback callback;
        TimePoint nextExecution;
        std::chrono::seconds interval;
        bool isRecurring;
    };

    std::unique_ptr<ParallelTaskManager> taskManager_;
    std::unordered_map<std::string, TaskInfo> tasks_;
    std::mutex tasksMutex_;
    std::condition_variable condition_;
    bool stop_;
    std::thread schedulerThread_;
}; 