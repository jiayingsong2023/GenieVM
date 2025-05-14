#include "common/scheduler.hpp"
#include "common/logger.hpp"
#include <chrono>
#include <thread>

Scheduler::Scheduler() : running_(false) {
}

Scheduler::~Scheduler() {
    stop();
}

bool Scheduler::scheduleTask(const std::string& taskId,
                           TimePoint scheduledTime,
                           TaskCallback callback) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    
    Task task;
    task.scheduledTime = scheduledTime;
    task.interval = 0;
    task.callback = callback;
    task.isPeriodic = false;
    
    tasks_[taskId] = task;
    condition_.notify_one();
    return true;
}

bool Scheduler::schedulePeriodicTask(const std::string& taskId,
                                   Duration interval,
                                   TaskCallback callback) {
    if (interval <= 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(tasksMutex_);
    
    Task task;
    task.scheduledTime = std::time(nullptr) + interval;
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

void Scheduler::processTasks() {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    
    auto now = std::time(nullptr);
    auto it = tasks_.begin();
    
    while (it != tasks_.end()) {
        if (it->second.scheduledTime <= now) {
            executeTask(it->first, it->second);
            
            if (it->second.isPeriodic) {
                it->second.scheduledTime = getNextExecutionTime(it->second);
                ++it;
            } else {
                it = tasks_.erase(it);
            }
        } else {
            ++it;
        }
    }
}

void Scheduler::executeTask(const std::string& taskId, const Task& task) {
    try {
        task.callback();
    } catch (const std::exception& e) {
        Logger::error("Task " + taskId + " failed: " + e.what());
    }
}

Scheduler::TimePoint Scheduler::getNextExecutionTime(const Task& task) {
    return std::time(nullptr) + task.interval;
}

void Scheduler::schedulerLoop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(tasksMutex_);
        
        if (tasks_.empty()) {
            condition_.wait(lock, [this] { return !running_ || !tasks_.empty(); });
            continue;
        }
        
        auto now = std::time(nullptr);
        auto nextTask = tasks_.begin();
        auto waitTime = nextTask->second.scheduledTime - now;
        
        if (waitTime > 0) {
            condition_.wait_for(lock, std::chrono::seconds(waitTime),
                              [this] { return !running_; });
            continue;
        }
        
        processTasks();
    }
} 