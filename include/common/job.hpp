#pragma once

#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>

// Callback type definitions
using ProgressCallback = std::function<void(int progress)>;
using StatusCallback = std::function<void(const std::string& status)>;

class Job {
public:
    enum class State {
        PENDING,
        RUNNING,
        PAUSED,
        COMPLETED,
        FAILED,
        CANCELLED
    };

    Job();
    virtual ~Job() = default;

    // Job control
    virtual bool start() = 0;
    virtual bool pause() = 0;
    virtual bool resume() = 0;
    virtual bool cancel() = 0;

    // Status queries
    virtual bool isRunning() const = 0;
    virtual bool isPaused() const = 0;
    virtual bool isCompleted() const = 0;
    virtual bool isFailed() const = 0;
    virtual bool isCancelled() const = 0;
    virtual int getProgress() const = 0;
    virtual std::string getStatus() const = 0;
    virtual std::string getError() const = 0;
    virtual std::string getId() const = 0;

    // Common status queries
    State getState() const { return state_; }

    // Callbacks
    void setProgressCallback(ProgressCallback callback) { progressCallback_ = callback; }
    void setStatusCallback(StatusCallback callback) { statusCallback_ = callback; }

protected:
    void updateProgress(int progress);
    void setError(const std::string& error);
    void setState(State state);
    void setStatus(const std::string& status);
    void setId(const std::string& id) { id_ = id; }
    std::string generateId() const;

    std::string id_;
    State state_{State::PENDING};
    std::string status_{"pending"};
    int progress_{0};
    std::string error_;
    ProgressCallback progressCallback_;
    StatusCallback statusCallback_;
    mutable std::mutex mutex_;
}; 