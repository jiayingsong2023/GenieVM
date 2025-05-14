#include "backup/backup_job.hpp"
#include "common/logger.hpp"
#include <chrono>
#include <thread>
#include <random>
#include <sstream>
#include <iomanip>

BackupJob::BackupJob(std::shared_ptr<VMwareConnection> connection, const BackupConfig& config)
    : connection_(connection)
    , config_(config)
    , status_(Status::PENDING)
    , progress_(0.0) {
    
    // Generate a unique ID for this job
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    id_ = ss.str();
}

BackupJob::~BackupJob() {
    if (status_ == Status::RUNNING) {
        stop();
    }
}

bool BackupJob::start() {
    if (status_ != Status::PENDING && status_ != Status::PAUSED) {
        setError("Cannot start job in current state");
        return false;
    }

    if (!prepareVM()) {
        return false;
    }

    snapshotId_ = createSnapshot();
    if (snapshotId_.empty()) {
        cleanupVM();
        return false;
    }

    setStatus(Status::RUNNING);
    return true;
}

bool BackupJob::stop() {
    if (status_ != Status::RUNNING && status_ != Status::PAUSED) {
        setError("Cannot stop job in current state");
        return false;
    }

    if (!snapshotId_.empty()) {
        removeSnapshot(snapshotId_);
    }

    cleanupVM();
    setStatus(Status::COMPLETED);
    return true;
}

bool BackupJob::pause() {
    if (status_ != Status::RUNNING) {
        setError("Cannot pause job in current state");
        return false;
    }

    setStatus(Status::PAUSED);
    return true;
}

bool BackupJob::resume() {
    if (status_ != Status::PAUSED) {
        setError("Cannot resume job in current state");
        return false;
    }

    setStatus(Status::RUNNING);
    return true;
}

bool BackupJob::cancel() {
    if (status_ == Status::COMPLETED || status_ == Status::FAILED) {
        setError("Cannot cancel job in current state");
        return false;
    }

    if (!snapshotId_.empty()) {
        removeSnapshot(snapshotId_);
    }

    cleanupVM();
    setStatus(Status::CANCELLED);
    return true;
}

BackupJob::Status BackupJob::getStatus() const {
    return status_;
}

std::string BackupJob::getId() const {
    return id_;
}

BackupConfig BackupJob::getConfig() const {
    return config_;
}

std::string BackupJob::getErrorMessage() const {
    return errorMessage_;
}

double BackupJob::getProgress() const {
    return progress_;
}

bool BackupJob::prepareVM() {
    // Implementation depends on VMwareConnection
    return true;
}

bool BackupJob::cleanupVM() {
    // Implementation depends on VMwareConnection
    return true;
}

std::string BackupJob::createSnapshot() {
    // Implementation depends on VMwareConnection
    return "snapshot-123";  // Placeholder
}

bool BackupJob::removeSnapshot(const std::string& snapshotId) {
    // Implementation depends on VMwareConnection
    return true;
}

void BackupJob::updateProgress(double progress) {
    progress_ = progress;
}

void BackupJob::setStatus(Status status) {
    status_ = status;
}

void BackupJob::setError(const std::string& error) {
    errorMessage_ = error;
    Logger::error("Backup job " + id_ + " error: " + error);
} 