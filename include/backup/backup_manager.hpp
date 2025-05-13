#pragma once

#include <string>
#include <memory>
#include <vector>
#include <future>
#include "common/vsphere_rest_client.hpp"
#include "backup/backup_config.hpp"
#include "backup/backup_job.hpp"

namespace vmware {

class BackupManager {
public:
    BackupManager(const std::string& vcenterHost, 
                 const std::string& username,
                 const std::string& password);
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

private:
    std::unique_ptr<VSphereRestClient> restClient_;
    std::vector<std::unique_ptr<BackupJob>> activeJobs_;
    mutable std::mutex jobsMutex_;

    bool prepareVMForBackup(const std::string& vmId);
    bool cleanupVMAfterBackup(const std::string& vmId);
    std::string createBackupSnapshot(const std::string& vmId);
    bool removeBackupSnapshot(const std::string& vmId, const std::string& snapshotId);
};

} // namespace vmware 