#include "common/scheduler.hpp"
#include <algorithm>

namespace vmware {

Scheduler::Scheduler() : running_(false) {}

Scheduler::~Scheduler() {
    stop();
}

bool Scheduler::scheduleTask(const std::string& taskId,
                           const TimePoint& scheduledTime,
                           TaskCallback callback) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    
    Task task;
    task.scheduledTime = scheduledTime;
    task.callback = callback;
    task.isPeriodic = false;
    task.interval = std::chrono::seconds(0);
    
    tasks_[taskId] = task;
    condition_.notify_one();
    
    return true;
}

bool Scheduler::schedulePeriodicTask(const std::string& taskId,
                                   const std::chrono::seconds& interval,
                                   TaskCallback callback) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    
    Task task;
    task.scheduledTime = std::chrono::system_clock::now() + interval;
    task.interval = interval;
    task.callback = callback;
    task.isPeriodic = true;
    
    tasks_[taskId] = task;
    condition_.notify_one();
    
    return true;
}

bool Scheduler::cancelTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    return tasks_.erase(taskId) > 0;
}

void Scheduler::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    schedulerThread_ = std::thread(&Scheduler::schedulerLoop, this);
}

void Scheduler::stop() {
    if (!running_) {
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        running_ = false;
    }
    
    condition_.notify_one();
    
    if (schedulerThread_.joinable()) {
        schedulerThread_.join();
    }
}

void Scheduler::schedulerLoop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(tasksMutex_);
        
        if (tasks_.empty()) {
            condition_.wait(lock, [this] { return !running_ || !tasks_.empty(); });
            continue;
        }
        
        // Find the next task to execute
        auto now = std::chrono::system_clock::now();
        auto nextTask = std::min_element(tasks_.begin(), tasks_.end(),
            [](const auto& a, const auto& b) {
                return a.second.scheduledTime < b.second.scheduledTime;
            });
        
        if (nextTask->second.scheduledTime > now) {
            // Wait until the next task is due
            condition_.wait_until(lock, nextTask->second.scheduledTime,
                [this] { return !running_; });
            continue;
        }
        
        // Execute the task
        std::string taskId = nextTask->first;
        Task task = nextTask->second;
        
        // Remove the task if it's not periodic
        if (!task.isPeriodic) {
            tasks_.erase(nextTask);
        } else {
            // Update the next execution time for periodic tasks
            task.scheduledTime = getNextExecutionTime(task);
            tasks_[taskId] = task;
        }
        
        // Release the lock while executing the task
        lock.unlock();
        executeTask(taskId, task);
    }
}

void Scheduler::executeTask(const std::string& taskId, const Task& task) {
    try {
        task.callback();
    } catch (const std::exception& e) {
        Logger::error("Error executing task " + taskId + ": " + std::string(e.what()));
    }
}

Scheduler::TimePoint Scheduler::getNextExecutionTime(const Task& task) {
    return std::chrono::system_clock::now() + task.interval;
}

} // namespace vmware 