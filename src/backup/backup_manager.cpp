#include "backup/backup_manager.hpp"
#include "backup/backup_job.hpp"
#include "common/logger.hpp"
#include <algorithm>
#include <stdexcept>

BackupManager::BackupManager() {
}

BackupManager::BackupManager(std::shared_ptr<VMwareConnection> connection)
    : connection_(connection) {
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
    if (!provider_) {
        lastError_ = "No provider available";
        return false;
    }

    auto job = createBackupJob(config);
    if (!job) {
        return false;
    }

    try {
        job->start();
        activeJobs_[job->getId()] = job;
        return true;
    } catch (const std::exception& e) {
        lastError_ = "Failed to start backup job: " + std::string(e.what());
        removeBackupJob(job->getId());
        return false;
    }
}

bool BackupManager::getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                                   std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) const {
    if (!provider_) {
        const_cast<BackupManager*>(this)->lastError_ = "No provider available";
        return false;
    }

    return provider_->getChangedBlocks(vmId, diskPath, changedBlocks);
}