#include "backup/restore_job.hpp"
#include "backup/disk_backup.hpp"
#include "common/vsphere_rest_client.hpp"
#include <filesystem>
#include <chrono>
#include <thread>

namespace vmware {

RestoreJob::RestoreJob(const std::string& vmId, const std::string& backupId, const BackupConfig& config)
    : vmId_(vmId)
    , backupId_(backupId)
    , config_(config)
    , status_(RestoreStatus::PENDING)
    , progress_(0.0)
    , cancelled_(false) {
}

RestoreJob::~RestoreJob() {
    stop();
}

bool RestoreJob::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != RestoreStatus::PENDING && status_ != RestoreStatus::PAUSED) {
        setError("Cannot start restore job in current state");
        return false;
    }

    if (!validateConfig()) {
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
        return false;
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

const BackupConfig& RestoreJob::getConfig() const {
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

bool RestoreJob::validateConfig() {
    if (config_.backupDir.empty()) {
        setError("Backup directory not specified");
        return false;
    }

    std::filesystem::path backupPath = std::filesystem::path(config_.backupDir) / backupId_;
    if (!std::filesystem::exists(backupPath)) {
        setError("Backup not found: " + backupPath.string());
        return false;
    }

    return true;
}

void RestoreJob::runRestore() {
    try {
        // Get VM disk paths
        auto diskPaths = vsphereClient_->getVMDiskPaths(vmId_);
        if (diskPaths.empty()) {
            setError("No disks found for VM " + vmId_);
            return;
        }

        // Create restore directory
        std::filesystem::path restorePath = std::filesystem::path(config_.backupDir) / backupId_;
        if (!std::filesystem::exists(restorePath)) {
            setError("Backup not found: " + restorePath.string());
            return;
        }

        // Restore each disk
        size_t totalDisks = diskPaths.size();
        size_t completedDisks = 0;

        for (const auto& diskPath : diskPaths) {
            if (cancelled_) {
                setError("Restore cancelled");
                return;
            }

            std::string backupDiskPath = (restorePath / std::filesystem::path(diskPath).filename()).string();
            
            // Create disk backup object for restore
            DiskBackup diskBackup(backupDiskPath, diskPath);
            if (!diskBackup.initialize()) {
                setError("Failed to initialize disk backup for restore");
                return;
            }

            // Restore disk
            if (!diskBackup.restore()) {
                setError("Failed to restore disk: " + diskPath);
                return;
            }

            completedDisks++;
            updateProgress(static_cast<double>(completedDisks) / totalDisks);
        }

        std::lock_guard<std::mutex> lock(mutex_);
        status_ = RestoreStatus::COMPLETED;
        progress_ = 1.0;
    } catch (const std::exception& e) {
        setError("Restore failed: " + std::string(e.what()));
    }
}

void RestoreJob::updateProgress(double progress) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_ = progress;
}

void RestoreJob::setError(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    errorMessage_ = message;
    status_ = RestoreStatus::FAILED;
    Logger::error("Restore failed for VM " + vmId_ + ": " + message);
}

} // namespace vmware 