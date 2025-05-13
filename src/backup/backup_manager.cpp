#include "backup/backup_manager.hpp"
#include "common/logger.hpp"
#include "common/thread_utils.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <type_traits>
#include <algorithm>
#include <stdexcept>

namespace vmware {

BackupManager::BackupManager(const std::string& vcenterHost, 
                           const std::string& username,
                           const std::string& password)
    : restClient_(std::make_unique<VSphereRestClient>(vcenterHost, username, password)) {
}

BackupManager::~BackupManager() = default;

bool BackupManager::startBackup(const std::string& vmId, const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(jobsMutex_);
    
    // Check if backup is already running
    auto it = std::find_if(activeJobs_.begin(), activeJobs_.end(),
                          [&vmId](const auto& job) { return job->getVMId() == vmId; });
    if (it != activeJobs_.end()) {
        return false;
    }

    // Prepare VM for backup
    if (!prepareVMForBackup(vmId)) {
        return false;
    }

    // Create backup job
    auto job = std::make_unique<BackupJob>(vmId, config);
    if (!job->start()) {
        cleanupVMAfterBackup(vmId);
        return false;
    }

    activeJobs_.push_back(std::move(job));
    return true;
}

bool BackupManager::stopBackup(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(jobsMutex_);
    
    auto it = std::find_if(activeJobs_.begin(), activeJobs_.end(),
                          [&vmId](const auto& job) { return job->getVMId() == vmId; });
    if (it == activeJobs_.end()) {
        return false;
    }

    if (!(*it)->stop()) {
        return false;
    }

    if (!cleanupVMAfterBackup(vmId)) {
        return false;
    }

    activeJobs_.erase(it);
    return true;
}

bool BackupManager::pauseBackup(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(jobsMutex_);
    
    auto it = std::find_if(activeJobs_.begin(), activeJobs_.end(),
                          [&vmId](const auto& job) { return job->getVMId() == vmId; });
    if (it == activeJobs_.end()) {
        return false;
    }

    return (*it)->pause();
}

bool BackupManager::resumeBackup(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(jobsMutex_);
    
    auto it = std::find_if(activeJobs_.begin(), activeJobs_.end(),
                          [&vmId](const auto& job) { return job->getVMId() == vmId; });
    if (it == activeJobs_.end()) {
        return false;
    }

    return (*it)->resume();
}

BackupStatus BackupManager::getBackupStatus(const std::string& vmId) const {
    std::lock_guard<std::mutex> lock(jobsMutex_);
    
    auto it = std::find_if(activeJobs_.begin(), activeJobs_.end(),
                          [&vmId](const auto& job) { return job->getVMId() == vmId; });
    if (it == activeJobs_.end()) {
        return BackupStatus::NOT_FOUND;
    }

    return (*it)->getStatus();
}

std::vector<BackupJob> BackupManager::getActiveBackups() const {
    std::lock_guard<std::mutex> lock(jobsMutex_);
    std::vector<BackupJob> result;
    result.reserve(activeJobs_.size());
    
    for (const auto& job : activeJobs_) {
        result.push_back(*job);
    }
    
    return result;
}

bool BackupManager::setBackupConfig(const std::string& vmId, const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(jobsMutex_);
    
    auto it = std::find_if(activeJobs_.begin(), activeJobs_.end(),
                          [&vmId](const auto& job) { return job->getVMId() == vmId; });
    if (it == activeJobs_.end()) {
        return false;
    }

    return (*it)->setConfig(config);
}

BackupConfig BackupManager::getBackupConfig(const std::string& vmId) const {
    std::lock_guard<std::mutex> lock(jobsMutex_);
    
    auto it = std::find_if(activeJobs_.begin(), activeJobs_.end(),
                          [&vmId](const auto& job) { return job->getVMId() == vmId; });
    if (it == activeJobs_.end()) {
        throw std::runtime_error("Backup job not found for VM: " + vmId);
    }

    return (*it)->getConfig();
}

bool BackupManager::prepareVMForBackup(const std::string& vmId) {
    // Get VM info
    auto vmInfo = restClient_->getVMInfo(vmId);
    if (vmInfo.is_null()) {
        return false;
    }

    // Enable CBT if needed
    if (!restClient_->enableCBT(vmId)) {
        return false;
    }

    return true;
}

bool BackupManager::cleanupVMAfterBackup(const std::string& vmId) {
    // Disable CBT
    if (!restClient_->disableCBT(vmId)) {
        return false;
    }

    return true;
}

std::string BackupManager::createBackupSnapshot(const std::string& vmId) {
    std::string snapshotName = "backup_" + std::to_string(std::time(nullptr));
    if (!restClient_->createSnapshot(vmId, snapshotName)) {
        return "";
    }
    return snapshotName;
}

bool BackupManager::removeBackupSnapshot(const std::string& vmId, const std::string& snapshotId) {
    return restClient_->removeSnapshot(vmId, snapshotId);
}

} // namespace vmware 