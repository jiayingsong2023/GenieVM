#include "backup/backup_job.hpp"
#include "common/logger.hpp"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

BackupJob::BackupJob(std::shared_ptr<BackupProvider> provider, const BackupConfig& config)
    : provider_(provider)
    , config_(config)
    , status_("pending")
    , progress_(0.0)
    , isRunning_(false)
    , isPaused_(false)
{
    id_ = generateId();
}

BackupJob::~BackupJob() {
    cancel();
}

bool BackupJob::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isRunning_) {
        return false;
    }

    status_ = "running";
    isRunning_ = true;
    return true;
}

bool BackupJob::cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning_) {
        return false;
    }

    isRunning_ = false;
    status_ = "cancelled";
    return true;
}

bool BackupJob::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning_ || isPaused_) {
        return false;
    }

    isPaused_ = true;
    status_ = "paused";
    return true;
}

bool BackupJob::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning_ || !isPaused_) {
        return false;
    }

    isPaused_ = false;
    status_ = "running";
    return true;
}

std::string BackupJob::getId() const {
    return id_;
}

std::string BackupJob::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

double BackupJob::getProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

std::string BackupJob::getError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_;
}

bool BackupJob::isRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isRunning_;
}

bool BackupJob::isPaused() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isPaused_;
}

BackupConfig BackupJob::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void BackupJob::setConfig(const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void BackupJob::setProgressCallback(ProgressCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    progressCallback_ = callback;
}

void BackupJob::setStatusCallback(StatusCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    statusCallback_ = callback;
}

bool BackupJob::verifyBackup() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning_) {
        setError("Cannot verify incomplete backup");
        return false;
    }

    try {
        if (!provider_->verifyBackup(id_)) {
            setError("Verification failed: " + provider_->getLastError());
            return false;
        }
        status_ = "verified";
        if (statusCallback_) {
            statusCallback_("Verification completed successfully");
        }
        return true;
    } catch (const std::exception& e) {
        setError(std::string("Verification failed: ") + e.what());
        return false;
    }
}

void BackupJob::updateProgress(double progress) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_ = progress;
    if (progressCallback_) {
        progressCallback_(static_cast<int>(progress));
    }
}

void BackupJob::setError(const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_ = error;
    status_ = "failed";
    if (statusCallback_) {
        statusCallback_(error);
    }
}

std::string BackupJob::generateId() const {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    
    std::stringstream ss;
    ss << std::hex << now_ms.count();
    return ss.str();
} 