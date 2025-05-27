#include "common/job_manager.hpp"
#include "backup/vmware/vmware_backup_provider.hpp"
#include "common/logger.hpp"
#include <algorithm>

JobManager::JobManager() {
    connection_ = std::make_shared<VMwareConnection>();
    provider_ = std::make_shared<VMwareBackupProvider>(connection_);
}

JobManager::JobManager(std::shared_ptr<VMwareConnection> connection)
    : connection_(connection) {
    if (connection_) {
        provider_ = std::make_shared<VMwareBackupProvider>(connection_);
    }
}

JobManager::~JobManager() {
    try {
        // Stop all running jobs
        stopAllJobs();
        
        // Clean up completed jobs
        cleanupCompletedJobs();
        
        // Disconnect from server
        disconnect();
        
        // Clear all job registries
        backupJobs_.clear();
        verifyJobs_.clear();
        restoreJobs_.clear();
        
        // Clear provider and connection
        provider_.reset();
        connection_.reset();
    } catch (const std::exception& e) {
        Logger::error("Error during JobManager cleanup: " + std::string(e.what()));
    }
}

bool JobManager::initialize() {
    if (!connection_) {
        lastError_ = "No connection available";
        return false;
    }
    return true;
}

bool JobManager::connect(const std::string& host, const std::string& username, const std::string& password) {
    if (!connection_) {
        lastError_ = "No connection available";
        return false;
    }
    return connection_->connect(host, username, password);
}

void JobManager::disconnect() {
    if (connection_) {
        connection_->disconnect();
    }
}

bool JobManager::isConnected() const {
    return connection_ && connection_->isConnected();
}

std::shared_ptr<BackupJob> JobManager::createBackupJob(const BackupConfig& config) {
    if (!provider_) {
        lastError_ = "No provider available";
        return nullptr;
    }

    auto taskManager = std::make_shared<ParallelTaskManager>();
    auto job = std::make_shared<BackupJob>(provider_, taskManager, config);
    backupJobs_[job->getId()] = job;
    return job;
}

std::shared_ptr<VerifyJob> JobManager::createVerifyJob(const VerifyConfig& config) {
    if (!provider_) {
        lastError_ = "No provider available";
        return nullptr;
    }

    auto taskManager = std::make_shared<ParallelTaskManager>();
    auto job = std::make_shared<VerifyJob>(provider_, taskManager, config);
    verifyJobs_[job->getId()] = job;
    return job;
}

std::shared_ptr<RestoreJob> JobManager::createRestoreJob(const RestoreConfig& config) {
    if (!provider_) {
        lastError_ = "No provider available";
        return nullptr;
    }

    auto taskManager = std::make_shared<ParallelTaskManager>();
    auto job = std::make_shared<RestoreJob>(provider_, taskManager, config);
    restoreJobs_[job->getId()] = job;
    return job;
}

std::vector<std::shared_ptr<BackupJob>> JobManager::getBackupJobs() const {
    std::vector<std::shared_ptr<BackupJob>> result;
    result.reserve(backupJobs_.size());
    for (const auto& pair : backupJobs_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<std::shared_ptr<VerifyJob>> JobManager::getVerifyJobs() const {
    std::vector<std::shared_ptr<VerifyJob>> result;
    result.reserve(verifyJobs_.size());
    for (const auto& pair : verifyJobs_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<std::shared_ptr<RestoreJob>> JobManager::getRestoreJobs() const {
    std::vector<std::shared_ptr<RestoreJob>> result;
    result.reserve(restoreJobs_.size());
    for (const auto& pair : restoreJobs_) {
        result.push_back(pair.second);
    }
    return result;
}

std::shared_ptr<BackupJob> JobManager::getBackupJob(const std::string& jobId) const {
    auto it = backupJobs_.find(jobId);
    return it != backupJobs_.end() ? it->second : nullptr;
}

std::shared_ptr<VerifyJob> JobManager::getVerifyJob(const std::string& jobId) const {
    auto it = verifyJobs_.find(jobId);
    return it != verifyJobs_.end() ? it->second : nullptr;
}

std::shared_ptr<RestoreJob> JobManager::getRestoreJob(const std::string& jobId) const {
    auto it = restoreJobs_.find(jobId);
    return it != restoreJobs_.end() ? it->second : nullptr;
}

bool JobManager::removeJob(const std::string& jobId) {
    // Try to find and remove from each job registry
    auto backupIt = backupJobs_.find(jobId);
    if (backupIt != backupJobs_.end()) {
        backupJobs_.erase(backupIt);
        return true;
    }

    auto verifyIt = verifyJobs_.find(jobId);
    if (verifyIt != verifyJobs_.end()) {
        verifyJobs_.erase(verifyIt);
        return true;
    }

    auto restoreIt = restoreJobs_.find(jobId);
    if (restoreIt != restoreJobs_.end()) {
        restoreJobs_.erase(restoreIt);
        return true;
    }

    return false;
}

void JobManager::cleanupCompletedJobs() {
    // Remove completed jobs from each registry
    auto removeCompleted = [](auto& jobs) {
        for (auto it = jobs.begin(); it != jobs.end();) {
            if (it->second->isCompleted() || it->second->isFailed() || it->second->isCancelled()) {
                it = jobs.erase(it);
            } else {
                ++it;
            }
        }
    };

    removeCompleted(backupJobs_);
    removeCompleted(verifyJobs_);
    removeCompleted(restoreJobs_);
}

void JobManager::stopAllJobs() {
    // Stop all jobs in each registry
    auto stopJobs = [](auto& jobs) {
        for (auto& pair : jobs) {
            pair.second->cancel();
        }
    };

    stopJobs(backupJobs_);
    stopJobs(verifyJobs_);
    stopJobs(restoreJobs_);
}

bool JobManager::getChangedBlocks(const std::string& vmId, const std::string& backupId,
                                std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) {
    if (!provider_) {
        lastError_ = "No provider available";
        return false;
    }

    // Get disk paths for the VM
    std::vector<std::string> diskPaths;
    if (!provider_->getVMDiskPaths(vmId, diskPaths)) {
        lastError_ = "Failed to get VM disk paths: " + provider_->getLastError();
        return false;
    }

    // Get changed blocks for each disk
    for (const auto& diskPath : diskPaths) {
        std::vector<std::pair<uint64_t, uint64_t>> diskBlocks;
        if (!provider_->getChangedBlocks(vmId, diskPath, diskBlocks)) {
            lastError_ = "Failed to get changed blocks for disk " + diskPath + ": " + provider_->getLastError();
            return false;
        }
        changedBlocks.insert(changedBlocks.end(), diskBlocks.begin(), diskBlocks.end());
    }

    return true;
}

bool JobManager::addJob(const std::shared_ptr<Job>& job) {
    if (!job) {
        lastError_ = "Invalid job pointer";
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    // Try to cast to specific job types and add to appropriate registry
    if (auto backupJob = std::dynamic_pointer_cast<BackupJob>(job)) {
        backupJobs_[job->getId()] = backupJob;
        return true;
    }
    if (auto verifyJob = std::dynamic_pointer_cast<VerifyJob>(job)) {
        verifyJobs_[job->getId()] = verifyJob;
        return true;
    }
    if (auto restoreJob = std::dynamic_pointer_cast<RestoreJob>(job)) {
        restoreJobs_[job->getId()] = restoreJob;
        return true;
    }

    lastError_ = "Unknown job type";
    return false;
} 