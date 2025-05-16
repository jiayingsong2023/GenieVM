#include "backup/backup_job.hpp"
#include "common/logger.hpp"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

BackupJob::BackupJob(std::shared_ptr<BackupProvider> provider, const BackupConfig& config)
    : provider_(provider)
    , config_(config)
    , status_(Status::PENDING)
    , progress_(0.0)
    , running_(false)
    , paused_(false)
{
    id_ = generateId();
}

BackupJob::~BackupJob() {
    cancel();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void BackupJob::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ == Status::RUNNING) {
        return;
    }

    status_ = Status::RUNNING;
    running_ = true;
    worker_ = std::thread(&BackupJob::workerFunction, this);
}

void BackupJob::cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ != Status::RUNNING) {
        return;
    }

    running_ = false;
    status_ = Status::CANCELLED;
    if (worker_.joinable()) {
        worker_.join();
    }
}

void BackupJob::verifyBackup() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ != Status::COMPLETED) {
        setError("Cannot verify incomplete backup");
        return;
    }

    try {
        provider_->verifyBackup(config_);
        updateStatus("Verification completed successfully");
    } catch (const std::exception& e) {
        setError(std::string("Verification failed: ") + e.what());
    }
}

std::string BackupJob::getId() const {
    return id_;
}

BackupJob::Status BackupJob::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

double BackupJob::getProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

std::string BackupJob::getErrorMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errorMessage_;
}

void BackupJob::setProgressCallback(std::function<void(int)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    progressCallback_ = callback;
}

void BackupJob::setStatusCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    statusCallback_ = callback;
}

void BackupJob::workerFunction() {
    try {
        updateStatus("Starting backup");
        provider_->initialize();
        
        while (running_ && !paused_) {
            // Perform backup operations
            double progress = provider_->getProgress();
            updateProgress(progress);
            
            if (progress >= 100.0) {
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (running_) {
            status_ = Status::COMPLETED;
            updateStatus("Backup completed successfully");
        }
    } catch (const std::exception& e) {
        setError(std::string("Backup failed: ") + e.what());
        status_ = Status::FAILED;
    }
}

void BackupJob::updateStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (statusCallback_) {
        statusCallback_(status);
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
    errorMessage_ = error;
    status_ = Status::FAILED;
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