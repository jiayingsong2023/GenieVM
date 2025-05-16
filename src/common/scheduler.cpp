#include "common/scheduler.hpp"
#include "common/logger.hpp"
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>

Scheduler::Scheduler()
    : running_(false) {
}

Scheduler::~Scheduler() {
    stop();
    if (schedulerThread_.joinable()) {
        schedulerThread_.join();
    }
}

bool Scheduler::scheduleTask(const std::string& taskId, 
                           TimePoint scheduledTime,
                           TaskCallback callback) {
    std::unique_lock<std::mutex> lock(tasksMutex_);
    tasks_[taskId] = {scheduledTime, 0, callback, false};
    condition_.notify_one();
    return true;
}

bool Scheduler::schedulePeriodicTask(const std::string& taskId,
                                   Duration interval,
                                   TaskCallback callback) {
    std::unique_lock<std::mutex> lock(tasksMutex_);
    auto now = std::time(nullptr);
    tasks_[taskId] = {now + interval, interval, callback, true};
    condition_.notify_one();
    return true;
}

bool Scheduler::cancelTask(const std::string& taskId) {
    std::unique_lock<std::mutex> lock(tasksMutex_);
    return tasks_.erase(taskId) > 0;
}

void Scheduler::start() {
    if (!running_) {
        running_ = true;
        schedulerThread_ = std::thread(&Scheduler::schedulerLoop, this);
    }
}

void Scheduler::stop() {
    if (running_) {
        running_ = false;
        condition_.notify_one();
    }
}

void Scheduler::processTasks() {
    std::unique_lock<std::mutex> lock(tasksMutex_);
    auto now = std::time(nullptr);
    
    for (auto it = tasks_.begin(); it != tasks_.end();) {
        if (it->second.scheduledTime <= now) {
            auto taskId = it->first;
            auto task = it->second;
            
            if (task.isPeriodic) {
                task.scheduledTime = now + task.interval;
                it->second = task;
                ++it;
            } else {
                it = tasks_.erase(it);
            }
            
            lock.unlock();
            executeTask(taskId, task);
            lock.lock();
        } else {
            ++it;
        }
    }
}

void Scheduler::executeTask(const std::string& taskId, const Task& task) {
    try {
        task.callback();
    } catch (const std::exception& e) {
        std::stringstream ss;
        ss << "Task " << taskId << " failed: " << e.what();
        Logger::error(ss.str());
    }
}

Scheduler::TimePoint Scheduler::getNextExecutionTime(const Task& task) {
    return task.scheduledTime;
}

void Scheduler::schedulerLoop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(tasksMutex_);
        
        if (tasks_.empty()) {
            condition_.wait(lock, [this] {
                return !running_ || !tasks_.empty();
            });
            continue;
        }
        
        auto now = std::time(nullptr);
        auto nextTask = std::min_element(tasks_.begin(), tasks_.end(),
            [](const auto& a, const auto& b) {
                return a.second.scheduledTime < b.second.scheduledTime;
            });
        
        if (nextTask->second.scheduledTime <= now) {
            auto taskId = nextTask->first;
            auto task = nextTask->second;
            
            if (task.isPeriodic) {
                task.scheduledTime = now + task.interval;
                nextTask->second = task;
            } else {
                tasks_.erase(nextTask);
            }
            
            lock.unlock();
            executeTask(taskId, task);
            lock.lock();
        } else {
            condition_.wait_for(lock, 
                std::chrono::seconds(nextTask->second.scheduledTime - now),
                [this] { return !running_; });
        }
    }
} 