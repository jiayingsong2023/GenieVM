#pragma once

#include <string>
#include <functional>
#include <mutex>
#include <map>
#include <atomic>
#include <ctime>
#include "common/logger.hpp"
#include "common/thread_utils.hpp"

namespace vmware {

class Scheduler {
public:
    using TaskCallback = std::function<void()>;
    using TimePoint = time_t;  // Using time_t instead of chrono time_point
    using Duration = int;      // Using seconds as int instead of chrono duration

    Scheduler();
    ~Scheduler();

    // Schedule a task to run at a specific time
    bool scheduleTask(const std::string& taskId, 
                     TimePoint scheduledTime,
                     TaskCallback callback);

    // Schedule a task to run periodically
    bool schedulePeriodicTask(const std::string& taskId,
                            Duration interval,
                            TaskCallback callback);

    // Cancel a scheduled task
    bool cancelTask(const std::string& taskId);

    // Start the scheduler
    void start();

    // Stop the scheduler
    void stop();

    // Process pending tasks - should be called periodically from main thread
    void processTasks();

private:
    struct Task {
        TimePoint scheduledTime;
        Duration interval;
        TaskCallback callback;
        bool isPeriodic;
    };

    std::map<std::string, Task> tasks_;
    std::mutex tasksMutex_;
    std::atomic<bool> running_;

    void executeTask(const std::string& taskId, const Task& task);
    TimePoint getNextExecutionTime(const Task& task);
};

} // namespace vmware 