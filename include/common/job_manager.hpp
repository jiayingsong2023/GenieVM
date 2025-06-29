#pragma once

#include "common/job.hpp"
#include "backup/backup_job.hpp"
#include "backup/verify_job.hpp"
#include "backup/restore_job.hpp"
#include "backup/backup_provider.hpp"
#include "backup/vm_config.hpp"
#include "common/logger.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <map>

class JobManager {
public:
    JobManager();
    ~JobManager();

    // Provider management
    void setProvider(BackupProvider* provider);
    bool initialize();
    bool connect(const std::string& host, const std::string& username, const std::string& password);
    void disconnect();
    bool isConnected() const;

    // Job management
    std::shared_ptr<BackupJob> createBackupJob(const BackupConfig& config);
    std::shared_ptr<VerifyJob> createVerifyJob(const VerifyConfig& config);
    std::shared_ptr<RestoreJob> createRestoreJob(const RestoreConfig& config);

    // Job queries
    std::vector<std::shared_ptr<BackupJob>> getBackupJobs() const;
    std::vector<std::shared_ptr<VerifyJob>> getVerifyJobs() const;
    std::vector<std::shared_ptr<RestoreJob>> getRestoreJobs() const;
    
    std::shared_ptr<BackupJob> getBackupJob(const std::string& jobId) const;
    std::shared_ptr<VerifyJob> getVerifyJob(const std::string& jobId) const;
    std::shared_ptr<RestoreJob> getRestoreJob(const std::string& jobId) const;

    // Job lifecycle management
    bool removeJob(const std::string& jobId);
    void cleanupCompletedJobs();
    void stopAllJobs();
    bool addJob(const std::shared_ptr<Job>& job);

    // Changed block tracking
    bool getChangedBlocks(const std::string& vmId, const std::string& backupId, 
                         std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks);

    // Error handling
    std::string getLastError() const { return lastError_; }
    void clearLastError() { lastError_.clear(); }

private:
    BackupProvider* provider_;      // Not owned by JobManager
    
    // Job registries
    std::unordered_map<std::string, std::shared_ptr<BackupJob>> backupJobs_;
    std::unordered_map<std::string, std::shared_ptr<VerifyJob>> verifyJobs_;
    std::unordered_map<std::string, std::shared_ptr<RestoreJob>> restoreJobs_;
    
    std::string lastError_;
    mutable std::mutex mutex_;
}; 