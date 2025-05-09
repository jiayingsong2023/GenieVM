#pragma once

#include <string>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <atomic>
#include "common/logger.hpp"

namespace vmware {

class Scheduler {
public:
    using TaskCallback = std::function<void()>;
    using TimePoint = std::chrono::system_clock::time_point;

    Scheduler();
    ~Scheduler();

    // Schedule a task to run at a specific time
    bool scheduleTask(const std::string& taskId, 
                     const TimePoint& scheduledTime,
                     TaskCallback callback);

    // Schedule a task to run periodically
    bool schedulePeriodicTask(const std::string& taskId,
                            const std::chrono::seconds& interval,
                            TaskCallback callback);

    // Cancel a scheduled task
    bool cancelTask(const std::string& taskId);

    // Start the scheduler
    void start();

    // Stop the scheduler
    void stop();

private:
    struct Task {
        TimePoint scheduledTime;
        std::chrono::seconds interval;
        TaskCallback callback;
        bool isPeriodic;
    };

    std::map<std::string, Task> tasks_;
    std::mutex tasksMutex_;
    std::condition_variable condition_;
    std::thread schedulerThread_;
    std::atomic<bool> running_;

    void schedulerLoop();
    void executeTask(const std::string& taskId, const Task& task);
    TimePoint getNextExecutionTime(const Task& task);
};

} // namespace vmware 