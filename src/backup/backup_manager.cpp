#include "backup/backup_manager.hpp"
#include "backup/backup_job.hpp"
#include "common/logger.hpp"
#include "backup/vmware/vmware_backup_provider.hpp"
#include <algorithm>
#include <stdexcept>
#include <mutex>

BackupManager::BackupManager() {
    // Don't create provider in default constructor
}

BackupManager::BackupManager(std::shared_ptr<VMwareConnection> connection)
    : connection_(connection) {
    if (connection_) {
        provider_ = std::make_unique<VMwareBackupProvider>(connection_);
    }
}

BackupManager::~BackupManager() {
    jobs_.clear();
}

std::shared_ptr<BackupJob> BackupManager::createBackupJob(const BackupConfig& config) {
    if (!provider_) {
        lastError_ = "No provider available";
        return nullptr;
    }

    auto sharedProvider = std::dynamic_pointer_cast<BackupProvider>(provider_);
    if (!sharedProvider) {
        lastError_ = "Failed to cast provider to BackupProvider";
        return nullptr;
    }

    auto job = std::make_shared<BackupJob>(sharedProvider, config);
    jobs_[job->getId()] = job;
    return job;
}

std::vector<std::shared_ptr<BackupJob>> BackupManager::getBackupJobs() const {
    std::vector<std::shared_ptr<BackupJob>> result;
    result.reserve(jobs_.size());
    for (const auto& pair : jobs_) {
        result.push_back(pair.second);
    }
    return result;
}

std::shared_ptr<BackupJob> BackupManager::getBackupJob(const std::string& jobId) const {
    auto it = jobs_.find(jobId);
    return it != jobs_.end() ? it->second : nullptr;
}

bool BackupManager::removeBackupJob(const std::string& jobId) {
    auto it = jobs_.find(jobId);
    if (it != jobs_.end()) {
        jobs_.erase(it);
        return true;
    }
    return false;
}

bool BackupManager::startBackup(const std::string& vmId, const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(statusMutex_);
    
    // Initialize backup status
    BackupStatus status;
    status.state = BackupState::InProgress;
    status.progress = 0.0;
    status.status = "Starting backup...";
    backupStatuses_[vmId] = status;

    if (!provider_->startBackup(vmId, config)) {
        status.state = BackupState::Failed;
        status.error = provider_->getLastError();
        backupStatuses_[vmId] = status;
        return false;
    }

    return true;
}

bool BackupManager::pauseBackup(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(statusMutex_);
    auto it = backupStatuses_.find(vmId);
    if (it == backupStatuses_.end()) {
        lastError_ = "No backup found for VM: " + vmId;
        return false;
    }

    if (it->second.state != BackupState::InProgress) {
        lastError_ = "Backup is not in progress";
        return false;
    }

    if (!provider_->pauseBackup(vmId)) {
        lastError_ = provider_->getLastError();
        return false;
    }

    it->second.state = BackupState::Paused;
    it->second.status = "Backup paused";
    return true;
}

bool BackupManager::resumeBackup(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(statusMutex_);
    auto it = backupStatuses_.find(vmId);
    if (it == backupStatuses_.find(vmId)) {
        lastError_ = "No backup found for VM: " + vmId;
        return false;
    }

    if (it->second.state != BackupState::Paused) {
        lastError_ = "Backup is not paused";
        return false;
    }

    if (!provider_->resumeBackup(vmId)) {
        lastError_ = provider_->getLastError();
        return false;
    }

    it->second.state = BackupState::InProgress;
    it->second.status = "Backup resumed";
    return true;
}

bool BackupManager::cancelBackup(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(statusMutex_);
    auto it = backupStatuses_.find(vmId);
    if (it == backupStatuses_.end()) {
        lastError_ = "No backup found for VM: " + vmId;
        return false;
    }

    if (!provider_->cancelBackup(vmId)) {
        lastError_ = provider_->getLastError();
        return false;
    }

    it->second.state = BackupState::Cancelled;
    it->second.status = "Backup cancelled";
    return true;
}

BackupStatus BackupManager::getBackupStatus(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(statusMutex_);
    auto it = backupStatuses_.find(vmId);
    if (it == backupStatuses_.end()) {
        BackupStatus status;
        status.state = BackupState::Failed;
        status.error = "No backup found for VM: " + vmId;
        return status;
    }

    // Update status from provider
    BackupStatus providerStatus = provider_->getBackupStatus(vmId);
    it->second.progress = providerStatus.progress;
    it->second.status = providerStatus.status;
    it->second.error = providerStatus.error;

    return it->second;
}

bool BackupManager::getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                                   std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) const {
    if (!provider_) {
        const_cast<BackupManager*>(this)->lastError_ = "No provider available";
        return false;
    }

    return provider_->getChangedBlocks(vmId, diskPath, changedBlocks);
}

std::string BackupManager::getLastError() const {
    return lastError_;
}

void BackupManager::clearLastError() {
    lastError_.clear();
}