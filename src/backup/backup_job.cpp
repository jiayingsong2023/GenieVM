#include "backup/backup_job.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <chrono>
#include <thread>

namespace vmware {

BackupJob::BackupJob(const std::string& vmId, const BackupConfig& config)
    : vmId_(vmId)
    , config_(config)
    , status_(BackupStatus::PENDING)
    , progress_(0.0)
    , shouldStop_(false) {
}

BackupJob::~BackupJob() {
    if (status_ == BackupStatus::RUNNING) {
        stop();
    }
}

bool BackupJob::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != BackupStatus::PENDING && status_ != BackupStatus::PAUSED) {
        return false;
    }

    if (!validateConfig()) {
        setError("Invalid backup configuration");
        status_ = BackupStatus::FAILED;
        return false;
    }

    // Create backup directory if it doesn't exist
    try {
        std::filesystem::create_directories(config_.backupDir);
    } catch (const std::filesystem::filesystem_error& e) {
        setError("Failed to create backup directory: " + std::string(e.what()));
        status_ = BackupStatus::FAILED;
        return false;
    }

    shouldStop_ = false;
    status_ = BackupStatus::RUNNING;
    progress_ = 0.0;
    errorMessage_.clear();

    // Start backup in a separate thread
    backupFuture_ = std::async(std::launch::async, [this]() {
        try {
            runBackup();
        } catch (const std::exception& e) {
            setError("Backup failed: " + std::string(e.what()));
            status_ = BackupStatus::FAILED;
        }
    });

    return true;
}

bool BackupJob::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != BackupStatus::RUNNING && status_ != BackupStatus::PAUSED) {
        return false;
    }

    shouldStop_ = true;
    status_ = BackupStatus::CANCELLED;

    if (backupFuture_.valid()) {
        backupFuture_.wait();
    }

    return true;
}

bool BackupJob::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != BackupStatus::RUNNING) {
        return false;
    }

    status_ = BackupStatus::PAUSED;
    return true;
}

bool BackupJob::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != BackupStatus::PAUSED) {
        return false;
    }

    status_ = BackupStatus::RUNNING;
    return true;
}

BackupStatus BackupJob::getStatus() const {
    return status_;
}

bool BackupJob::setConfig(const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ == BackupStatus::RUNNING) {
        return false;
    }

    config_ = config;
    return true;
}

double BackupJob::getProgress() const {
    return progress_;
}

std::string BackupJob::getErrorMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errorMessage_;
}

void BackupJob::runBackup() {
    // TODO: Implement actual backup logic using VDDK
    // This is a placeholder that simulates backup progress
    for (int i = 0; i <= 100 && !shouldStop_; ++i) {
        if (status_ == BackupStatus::PAUSED) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        
        updateProgress(i / 100.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (shouldStop_) {
        status_ = BackupStatus::CANCELLED;
    } else {
        status_ = BackupStatus::COMPLETED;
        progress_ = 1.0;
    }
}

bool BackupJob::validateConfig() const {
    if (config_.backupDir.empty()) {
        return false;
    }

    if (config_.compressionLevel < 0 || config_.compressionLevel > 9) {
        return false;
    }

    if (config_.maxConcurrentDisks < 1) {
        return false;
    }

    if (config_.retentionDays < 0) {
        return false;
    }

    if (config_.maxBackups < 1) {
        return false;
    }

    return true;
}

void BackupJob::updateProgress(double progress) {
    progress_ = progress;
}

void BackupJob::setError(const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    errorMessage_ = error;
    Logger::error("Backup error for VM " + vmId_ + ": " + error);
}

} // namespace vmware 