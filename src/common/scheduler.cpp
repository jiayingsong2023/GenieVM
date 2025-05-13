#include "common/scheduler.hpp"
#include "common/logger.hpp"
#include "common/thread_utils.hpp"
#include <algorithm>

namespace vmware {

Scheduler::Scheduler() : running_(false) {}

Scheduler::~Scheduler() {
    stop();
}

bool Scheduler::scheduleTask(const std::string& taskId,
                           TimePoint scheduledTime,
                           TaskCallback callback) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    
    Task task;
    task.scheduledTime = scheduledTime;
    task.interval = 0;  // Non-periodic task
    task.callback = callback;
    task.isPeriodic = false;
    
    tasks_[taskId] = task;
    condition_.notify_one();
    return true;
}

bool Scheduler::schedulePeriodicTask(const std::string& taskId,
                                   Duration interval,
                                   TaskCallback callback) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    
    Task task;
    task.scheduledTime = thread_utils::get_current_time() + interval;
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
        auto now = thread_utils::get_current_time();
        auto nextTask = std::min_element(tasks_.begin(), tasks_.end(),
            [](const auto& a, const auto& b) {
                return a.second.scheduledTime < b.second.scheduledTime;
            });
        
        if (nextTask != tasks_.end()) {
            if (thread_utils::has_time_passed(nextTask->second.scheduledTime)) {
                // Execute the task
                Task task = nextTask->second;
                lock.unlock();
                executeTask(nextTask->first, task);
                lock.lock();
                
                // Update or remove the task
                if (task.isPeriodic) {
                    task.scheduledTime = thread_utils::get_current_time() + task.interval;
                    tasks_[nextTask->first] = task;
                } else {
                    tasks_.erase(nextTask);
                }
            } else {
                // Wait until the next task is due
                time_t waitTime = nextTask->second.scheduledTime - now;
                if (waitTime > 0) {
                    // Sleep in small intervals to allow for cancellation
                    while (waitTime > 0 && running_) {
                        lock.unlock();
                        thread_utils::sleep_for_seconds(1);
                        lock.lock();
                        waitTime--;
                    }
                }
            }
        }
    }
}

void Scheduler::executeTask(const std::string& taskId, const Task& task) {
    try {
        task.callback();
    } catch (const std::exception& e) {
        Logger::error("Error executing task " + taskId + ": " + e.what());
    }
}

TimePoint Scheduler::getNextExecutionTime(const Task& task) {
    return thread_utils::get_current_time() + task.interval;
}

} // namespace vmware 