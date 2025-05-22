#pragma once

#include "backup/backup_provider.hpp"
#include "backup/vmware/vmware_backup_provider.hpp"
#include "common/vmware_connection.hpp"
#include "backup/vm_config.hpp"
#include "backup/backup_job.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <map>
#include <mutex>

// Type definitions
using ProgressCallback = std::function<void(int)>;
using StatusCallback = std::function<void(const std::string&)>;

class BackupManager {
public:
    BackupManager();
    explicit BackupManager(std::shared_ptr<VMwareConnection> connection);
    ~BackupManager();

    // Connection management
    bool initialize();
    bool connect(const std::string& host, const std::string& username, const std::string& password, const std::string& type);
    void disconnect();
    bool isConnected() const;

    // Error handling
    std::string getLastError() const;

    // VM management
    std::vector<std::string> listVMs() const;
    bool getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) const;
    bool getVMInfo(const std::string& vmId, std::string& name, std::string& status) const;

    // Backup operations
    bool startBackup(const std::string& vmId, const BackupConfig& config);
    bool cancelBackup(const std::string& backupId);
    bool pauseBackup(const std::string& backupId);
    bool resumeBackup(const std::string& backupId);
    bool getBackupStatus(const std::string& backupId, std::string& status, double& progress) const;

    // Restore operations
    bool startRestore(const std::string& vmId, const std::string& backupId);
    bool cancelRestore(const std::string& restoreId);
    bool pauseRestore(const std::string& restoreId);
    bool resumeRestore(const std::string& restoreId);
    bool getRestoreStatus(const std::string& restoreId, std::string& status, double& progress) const;

    // CBT operations
    bool enableCBT(const std::string& vmId);
    bool disableCBT(const std::string& vmId);
    bool isCBTEnabled(const std::string& vmId) const;
    bool getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                         std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) const;

    // Job management
    std::shared_ptr<BackupJob> createBackupJob(const BackupConfig& config);
    std::vector<std::shared_ptr<BackupJob>> getBackupJobs() const;
    std::shared_ptr<BackupJob> getBackupJob(const std::string& jobId) const;
    bool removeBackupJob(const std::string& jobId);

    // Callbacks
    void setProgressCallback(ProgressCallback callback);
    void setStatusCallback(StatusCallback callback);

    // Error handling
    void clearLastError();

private:
    std::shared_ptr<VMwareConnection> connection_;
    std::shared_ptr<VMwareBackupProvider> provider_;
    std::unordered_map<std::string, std::shared_ptr<BackupJob>> jobs_;
    std::string lastError_;
    ProgressCallback progressCallback_;
    StatusCallback statusCallback_;
    std::map<std::string, std::shared_ptr<BackupJob>> activeJobs_;
    mutable std::mutex mutex_;

    // Helper methods
    bool createProvider(const std::string& type);
}; 