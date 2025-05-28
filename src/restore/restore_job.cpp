#include "backup/restore_job.hpp"
#include "backup/backup_provider.hpp"
#include "backup/vm_config.hpp"
#include "common/parallel_task_manager.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>

using namespace std::filesystem;

RestoreJob::RestoreJob(BackupProvider* provider,
                      std::shared_ptr<ParallelTaskManager> taskManager,
                      const RestoreConfig& config)
    : provider_(provider)
    , taskManager_(taskManager)
    , config_(config) {
    // Generate a unique job ID
    setId(generateId());
    setStatus("pending");
}

RestoreJob::~RestoreJob() {
    if (isRunning()) {
        cancel();
    }
}

bool RestoreJob::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isRunning()) {
        return false;
    }

    setState(State::RUNNING);
    setStatus("Starting restore");
    updateProgress(0);

    // Start restore in a separate thread
    std::thread([this]() {
        try {
            bool success = true;
            for (const auto& diskConfig : config_.diskConfigs) {
                if (!restoreDisk(diskConfig.path)) {
                    success = false;
                    break;
                }
            }
            
            if (!success) {
                setError("Restore failed: " + provider_->getLastError());
                setState(State::FAILED);
            } else {
                setState(State::COMPLETED);
                setStatus("Restore completed successfully");
                updateProgress(100);
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_);
            setError(std::string("Restore failed: ") + e.what());
            setState(State::FAILED);
        }
    }).detach();

    return true;
}

bool RestoreJob::cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning()) {
        return false;
    }
    setState(State::CANCELLED);
    setStatus("Restore cancelled");
    return true;
}

bool RestoreJob::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning() || isPaused()) {
        return false;
    }
    setState(State::PAUSED);
    setStatus("Restore paused");
    return true;
}

bool RestoreJob::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning() || !isPaused()) {
        return false;
    }
    setState(State::RUNNING);
    setStatus("Restore resumed");
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

std::string RestoreJob::getVMId() const {
    return config_.vmId;
}

std::string RestoreJob::getBackupId() const {
    return config_.backupId;
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

        // Perform restore
        if (!provider_->restoreDisk(config_.vmId, diskPath, config_)) {
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        setError(e.what());
        return false;
    }
}

void RestoreJob::updateOverallProgress() {
    if (diskProgress_.empty()) {
        updateProgress(0);
        return;
    }

    int totalProgress = 0;
    for (const auto& pair : diskProgress_) {
        totalProgress += pair.second;
    }
    updateProgress(totalProgress / diskProgress_.size());
}

void RestoreJob::handleDiskTaskCompletion(const std::string& diskPath, bool success, const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!success) {
        setError("Failed to restore disk " + diskPath + ": " + error);
        setState(State::FAILED);
        return;
    }

    diskProgress_[diskPath] = 100;
    updateOverallProgress();
} 
