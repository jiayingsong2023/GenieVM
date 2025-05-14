#include "backup/backup_manager.hpp"
#include "common/logger.hpp"
#include <algorithm>
#include <stdexcept>

BackupManager::BackupManager(std::shared_ptr<VMwareConnection> connection)
    : connection_(connection) {
}

BackupManager::~BackupManager() {
    // Clean up any remaining jobs
    jobs_.clear();
}

std::shared_ptr<BackupJob> BackupManager::createBackupJob(const BackupConfig& config) {
    auto job = std::make_shared<BackupJob>(connection_, config);
    jobs_.push_back(job);
    return job;
}

std::vector<std::shared_ptr<BackupJob>> BackupManager::getBackupJobs() const {
    return jobs_;
}

std::shared_ptr<BackupJob> BackupManager::getBackupJob(const std::string& jobId) const {
    auto it = std::find_if(jobs_.begin(), jobs_.end(),
                          [&jobId](const auto& job) {
                              return job->getId() == jobId;
                          });
    return it != jobs_.end() ? *it : nullptr;
}

bool BackupManager::removeBackupJob(const std::string& jobId) {
    auto it = std::find_if(jobs_.begin(), jobs_.end(),
                          [&jobId](const auto& job) {
                              return job->getId() == jobId;
                          });
    if (it != jobs_.end()) {
        jobs_.erase(it);
        return true;
    }
    return false;
}