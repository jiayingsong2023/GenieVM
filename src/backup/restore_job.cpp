#include "backup/restore_job.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <stdexcept>
#include <chrono>
#include <thread>

RestoreJob::RestoreJob(const std::string& vmId, const std::string& backupId, const RestoreConfig& config)
    : vmId_(vmId)
    , backupId_(backupId)
    , config_(config)
    , status_(RestoreStatus::PENDING)
    , progress_(0.0)
    , cancelled_(false)
    , vsphereClient_(std::make_shared<VSphereRestClient>(
        config.vsphereHost,
        config.vsphereUsername,
        config.vspherePassword)) {
}

RestoreJob::~RestoreJob() {
    if (status_ == RestoreStatus::RUNNING) {
        stop();
    }
}

bool RestoreJob::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ == RestoreStatus::RUNNING) {
        Logger::error("Restore already in progress");
        return false;
    }

    if (!validateConfig()) {
        status_ = RestoreStatus::FAILED;
        return false;
    }

    status_ = RestoreStatus::RUNNING;
    cancelled_ = false;
    restoreFuture_ = std::async(std::launch::async, &RestoreJob::runRestore, this);
    return true;
}

bool RestoreJob::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != RestoreStatus::RUNNING && status_ != RestoreStatus::PAUSED) {
        return true;
    }

    cancelled_ = true;
    if (restoreFuture_.valid()) {
        restoreFuture_.wait();
    }
    status_ = RestoreStatus::CANCELLED;
    return true;
}

bool RestoreJob::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != RestoreStatus::RUNNING) {
        return false;
    }

    status_ = RestoreStatus::PAUSED;
    return true;
}

bool RestoreJob::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != RestoreStatus::PAUSED) {
        return false;
    }

    status_ = RestoreStatus::RUNNING;
    return true;
}

RestoreStatus RestoreJob::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

std::string RestoreJob::getVMId() const {
    return vmId_;
}

std::string RestoreJob::getBackupId() const {
    return backupId_;
}

const RestoreConfig& RestoreJob::getConfig() const {
    return config_;
}

double RestoreJob::getProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

std::string RestoreJob::getErrorMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errorMessage_;
}

void RestoreJob::runRestore() {
    try {
        // TODO: Implement actual restore logic
        // This should use vsphereClient_ to perform the restore
        // and call updateProgress() periodically
        
        if (cancelled_) {
            status_ = RestoreStatus::CANCELLED;
            return;
        }

        status_ = RestoreStatus::COMPLETED;
        updateProgress(1.0);
    } catch (const std::exception& e) {
        setError(e.what());
        status_ = RestoreStatus::FAILED;
    }
}

bool RestoreJob::validateConfig() {
    // Validate VM ID
    if (config_.vmId.empty()) {
        errorMessage_ = "VM ID is required";
        return false;
    }

    // Validate backup ID
    if (config_.backupId.empty()) {
        errorMessage_ = "Backup ID is required";
        return false;
    }

    // Validate target datastore
    if (config_.targetDatastore.empty()) {
        errorMessage_ = "Target datastore is required";
        return false;
    }

    // Validate target resource pool
    if (config_.targetResourcePool.empty()) {
        errorMessage_ = "Target resource pool is required";
        return false;
    }

    // Validate disk configurations
    if (config_.diskConfigs.empty()) {
        errorMessage_ = "At least one disk configuration is required";
        return false;
    }

    for (const auto& diskConfig : config_.diskConfigs) {
        // Validate disk path
        if (diskConfig.path.empty()) {
            errorMessage_ = "Disk path is required for all disks";
            return false;
        }

        // Validate disk size
        if (diskConfig.sizeKB <= 0) {
            errorMessage_ = "Disk size must be greater than 0";
            return false;
        }

        // Validate disk format
        if (diskConfig.format.empty()) {
            errorMessage_ = "Disk format is required";
            return false;
        }

        // Validate disk type
        if (diskConfig.type.empty()) {
            errorMessage_ = "Disk type is required";
            return false;
        }
    }

    return true;
}

void RestoreJob::updateProgress(double progress) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_ = progress;
}

void RestoreJob::setError(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    errorMessage_ = message;
} 