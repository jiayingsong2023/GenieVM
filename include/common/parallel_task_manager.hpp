#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <atomic>
#include <chrono>
#include <unordered_map>

class TaskProgress {
public:
    void updateProgress(double progress) {
        progress_.store(progress, std::memory_order_relaxed);
    }
    
    double getProgress() const {
        return progress_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<double> progress_{0.0};
};

class TaskHandle {
public:
    void cancel() { cancelled_.store(true, std::memory_order_relaxed); }
    bool isCancelled() const { return cancelled_.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> cancelled_{false};
};

struct TaskStats {
    size_t totalTasks{0};
    size_t completedTasks{0};
    size_t failedTasks{0};
    size_t cancelledTasks{0};
    double averageTaskTime{0.0};
    size_t currentQueueSize{0};
};

enum class TaskPriority {
    LOW,
    NORMAL,
    HIGH
};

class ParallelTaskManager {
public:
    explicit ParallelTaskManager(size_t numThreads = std::thread::hardware_concurrency());
    ~ParallelTaskManager();

    // Add a task to the queue with priority
    template<typename F, typename... Args>
    auto addTask(F&& f, Args&&... args, TaskPriority priority = TaskPriority::NORMAL) 
        -> std::future<typename std::result_of<F(Args...)>::type>;

    // Add a task with progress tracking
    template<typename F, typename... Args>
    std::pair<std::future<typename std::result_of<F(Args...)>::type>, std::shared_ptr<TaskProgress>>
    addTaskWithProgress(F&& f, Args&&... args);

    // Add a cancellable task
    template<typename F, typename... Args>
    std::pair<std::future<typename std::result_of<F(Args...)>::type>, TaskHandle>
    addCancellableTask(F&& f, Args&&... args);

    // Add a task with dependencies
    template<typename F, typename... Args>
    auto addDependentTask(F&& f, Args&&... args,
                         const std::vector<std::future<void>>& dependencies)
        -> std::future<typename std::result_of<F(Args...)>::type>;

    // Wait for all tasks to complete
    void waitForAll();

    // Get the number of active threads
    size_t getActiveThreadCount() const;

    // Get task statistics
    TaskStats getStats() const;

private:
    struct Task {
        std::function<void()> func;
        TaskPriority priority;
        std::chrono::steady_clock::time_point startTime;
        TaskProgress* progress;
        TaskHandle* handle;
        std::vector<std::future<void>> dependencies;
    };

    void workerThread();
    void stop();
    void updateStats(const Task& task, bool success, bool cancelled);
    bool checkDependencies(const Task& task);

    std::vector<std::thread> workers_;
    std::priority_queue<Task, std::vector<Task>, 
        std::function<bool(const Task&, const Task&)>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    bool stop_;

    // Statistics
    mutable std::mutex statsMutex_;
    TaskStats stats_;
    std::atomic<size_t> activeTasks_{0};
};

template<typename F, typename... Args>
auto ParallelTaskManager::addTask(F&& f, Args&&... args, TaskPriority priority) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        if (stop_) {
            throw std::runtime_error("Cannot add task to stopped task manager");
        }
        
        Task t{
            [task](){ (*task)(); },
            priority,
            std::chrono::steady_clock::now(),
            nullptr,
            nullptr,
            {}
        };
        
        tasks_.push(std::move(t));
        stats_.totalTasks++;
        stats_.currentQueueSize++;
    }
    
    condition_.notify_one();
    return result;
}

template<typename F, typename... Args>
std::pair<std::future<typename std::result_of<F(Args...)>::type>, std::shared_ptr<TaskProgress>>
ParallelTaskManager::addTaskWithProgress(F&& f, Args&&... args) {
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto progress = std::make_shared<TaskProgress>();
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        if (stop_) {
            throw std::runtime_error("Cannot add task to stopped task manager");
        }
        
        Task t{
            [task](){ (*task)(); },
            TaskPriority::NORMAL,
            std::chrono::steady_clock::now(),
            progress.get(),
            nullptr,
            {}
        };
        
        tasks_.push(std::move(t));
        stats_.totalTasks++;
        stats_.currentQueueSize++;
    }
    
    condition_.notify_one();
    return {std::move(result), progress};
}

template<typename F, typename... Args>
std::pair<std::future<typename std::result_of<F(Args...)>::type>, TaskHandle>
ParallelTaskManager::addCancellableTask(F&& f, Args&&... args) {
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto handle = std::make_shared<TaskHandle>();
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        [handle, f = std::forward<F>(f), args...]() {
            if (handle->isCancelled()) {
                throw std::runtime_error("Task was cancelled");
            }
            return f(std::forward<Args>(args)...);
        }
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        if (stop_) {
            throw std::runtime_error("Cannot add task to stopped task manager");
        }
        
        Task t{
            [task](){ (*task)(); },
            TaskPriority::NORMAL,
            std::chrono::steady_clock::now(),
            nullptr,
            handle.get(),
            {}
        };
        
        tasks_.push(std::move(t));
        stats_.totalTasks++;
        stats_.currentQueueSize++;
    }
    
    condition_.notify_one();
    return {std::move(result), *handle};
}

template<typename F, typename... Args>
auto ParallelTaskManager::addDependentTask(F&& f, Args&&... args,
                                         const std::vector<std::future<void>>& dependencies)
    -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        if (stop_) {
            throw std::runtime_error("Cannot add task to stopped task manager");
        }
        
        Task t{
            [task](){ (*task)(); },
            TaskPriority::NORMAL,
            std::chrono::steady_clock::now(),
            nullptr,
            nullptr,
            dependencies
        };
        
        tasks_.push(std::move(t));
        stats_.totalTasks++;
        stats_.currentQueueSize++;
    }
    
    condition_.notify_one();
    return result;
} 