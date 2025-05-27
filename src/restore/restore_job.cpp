#include "backup/restore_job.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <iomanip>

RestoreJob::RestoreJob(std::shared_ptr<BackupProvider> provider,
                      std::shared_ptr<ParallelTaskManager> taskManager,
                      const RestoreConfig& config)
    : provider_(provider)
    , taskManager_(taskManager)
    , config_(config) {
    setId(generateId());
    setStatus("pending");
}

RestoreJob::~RestoreJob() {
    if (isRunning()) {
        cancel();
    }
}

bool RestoreJob::start() {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (isRunning()) {
            return false;
        }

        // Get disk paths for the VM
        if (!provider_->getVMDiskPaths(config_.vmId, diskPaths_)) {
            setError("Failed to get VM disk paths: " + provider_->getLastError());
            return false;
        }

        // Start restore tasks for each disk
        for (const auto& diskPath : diskPaths_) {
            auto taskResult = taskManager_->addTaskWithProgress([this, diskPath]() {
                return restoreDisk(diskPath);
            });

            // Store the future and initialize progress
            diskTasks_[diskPath] = std::move(taskResult.first);
            diskProgress_[diskPath] = 0;

            // Monitor progress in a separate thread
            std::thread([this, diskPath, progressPtr = taskResult.second]() {
                while (diskProgress_[diskPath] < 100) {
                    diskProgress_[diskPath] = static_cast<int>(progressPtr->getProgress() * 100);
                    updateOverallProgress();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }).detach();
        }

        setState(State::RUNNING);
        setStatus("running");
        updateProgress(0);
        return true;
    } catch (const std::exception& e) {
        setError("Failed to start restore job: " + std::string(e.what()));
        return false;
    }
}

bool RestoreJob::cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning()) {
        return false;
    }

    // Cancel all disk tasks
    for (auto& [diskPath, future] : diskTasks_) {
        if (future.valid()) {
            future.wait();
        }
    }

    setState(State::CANCELLED);
    setStatus("cancelled");
    return true;
}

bool RestoreJob::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning() || isPaused()) {
        return false;
    }

    setState(State::PAUSED);
    setStatus("paused");
    return true;
}

bool RestoreJob::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning() || !isPaused()) {
        return false;
    }

    setState(State::RUNNING);
    setStatus("running");
    return true;
}

bool RestoreJob::isRunning() const {
    return getState() == State::RUNNING;
}

bool RestoreJob::isPaused() const {
    return getState() == State::PAUSED;
}

bool RestoreJob::isCompleted() const {
    return getState() == State::COMPLETED;
}

bool RestoreJob::isFailed() const {
    return getState() == State::FAILED;
}

bool RestoreJob::isCancelled() const {
    return getState() == State::CANCELLED;
}

int RestoreJob::getProgress() const {
    return Job::getProgress();
}

std::string RestoreJob::getStatus() const {
    return Job::getStatus();
}

std::string RestoreJob::getError() const {
    return Job::getError();
}

std::string RestoreJob::getId() const {
    return Job::getId();
}

bool RestoreJob::restoreDisk(const std::string& diskPath) {
    try {
        // Check if job is paused
        while (isPaused()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Check if job is cancelled
        if (!isRunning()) {
            return false;
        }

        // Perform disk restore
        if (!provider_->restoreDisk(config_.vmId, diskPath, config_)) {
            handleDiskTaskCompletion(diskPath, false,
                "Failed to restore disk: " + provider_->getLastError());
            return false;
        }

        handleDiskTaskCompletion(diskPath, true, "");
        return true;
    } catch (const std::exception& e) {
        handleDiskTaskCompletion(diskPath, false, e.what());
        return false;
    }
}

void RestoreJob::updateOverallProgress() {
    int totalProgress = 0;
    for (const auto& [diskPath, progress] : diskProgress_) {
        totalProgress += progress;
    }
    totalProgress /= diskProgress_.size();
    updateProgress(totalProgress);
}

void RestoreJob::handleDiskTaskCompletion(const std::string& diskPath, bool success, const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!success) {
        setError("Disk restore failed: " + error);
        setState(State::FAILED);
        setStatus("failed");
        return;
    }

    // Check if all disks are completed
    bool allCompleted = true;
    for (const auto& [path, future] : diskTasks_) {
        if (future.valid() && future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            allCompleted = false;
            break;
        }
    }

    if (allCompleted) {
        setState(State::COMPLETED);
        setStatus("completed");
        updateProgress(100);
    }
}

std::string RestoreJob::getVMId() const {
    return config_.vmId;
}

std::string RestoreJob::getBackupId() const {
    return config_.backupId;
} 
