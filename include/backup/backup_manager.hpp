#pragma once

#include <string>
#include <memory>
#include <vector>
#include <future>
#include "common/vsphere_rest_client.hpp"
#include "backup/backup_config.hpp"
#include "backup/backup_job.hpp"
#include <mutex>

namespace vmware {

class BackupManager {
public:
    BackupManager(std::unique_ptr<VSphereRestClient> restClient);
    ~BackupManager();

    // Backup operations
    bool startBackup(const std::string& vmId, const BackupConfig& config);
    bool stopBackup(const std::string& vmId);
    bool pauseBackup(const std::string& vmId);
    bool resumeBackup(const std::string& vmId);
    
    // Status and monitoring
    BackupStatus getBackupStatus(const std::string& vmId) const;
    std::vector<BackupJob> getActiveBackups() const;
    
    // Configuration
    bool setBackupConfig(const std::string& vmId, const BackupConfig& config);
    BackupConfig getBackupConfig(const std::string& vmId) const;

    void cancelBackup(const std::string& vmId);
    std::vector<BackupJob*> getActiveJobs();

private:
    bool prepareVMForBackup(const std::string& vmId);
    bool cleanupVMAfterBackup(const std::string& vmId);
    std::string createBackupSnapshot(const std::string& vmId);
    bool removeBackupSnapshot(const std::string& vmId, const std::string& snapshotId);

    std::unique_ptr<VSphereRestClient> restClient_;
    std::vector<std::unique_ptr<BackupJob>> activeJobs_;
    std::mutex mutex_;
};

} // namespace vmware 