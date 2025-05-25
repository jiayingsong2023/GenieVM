#pragma once

#include "backup/backup_provider.hpp"
#include "backup/backup_job.hpp"
#include "backup/vm_config.hpp"
#include "common/vmware_connection.hpp"
#include "common/backup_status.hpp"
#include "common/logger.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <vixDiskLib.h>
#include <unordered_map>
#include <set>
#include <filesystem>

// Type definitions
using ProgressCallback = std::function<void(int)>;
using StatusCallback = std::function<void(const std::string&)>;

class VMwareBackupProvider : public BackupProvider, public std::enable_shared_from_this<VMwareBackupProvider> {
public:
    explicit VMwareBackupProvider(std::shared_ptr<VMwareConnection> connection);
    ~VMwareBackupProvider() override;

    // Initialization
    bool initialize();

    // Connection management
    bool connect(const std::string& host, const std::string& username, const std::string& password) override;
    void disconnect() override;
    bool isConnected() const override;

    // VM operations
    std::vector<std::string> listVMs() const;
    bool getVMInfo(const std::string& vmId, std::string& name, std::string& status) const;
    bool getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) const override;
    bool backupDisk(const std::string& vmId, const std::string& diskPath, const BackupConfig& config) override;
    bool restoreDisk(const std::string& vmId, const std::string& diskPath, const RestoreConfig& config) override;
    bool verifyDisk(const std::string& diskPath) override;
    bool getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                         std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) override;

    // Snapshot operations
    bool createSnapshot(const std::string& vmId);
    bool removeSnapshot();
    void cleanupSnapshot();

    // Backup operations
    bool startBackup(const std::string& vmId, const BackupConfig& config);
    bool cancelBackup(const std::string& vmId);
    bool pauseBackup(const std::string& vmId);
    bool resumeBackup(const std::string& vmId);
    BackupStatus getBackupStatus(const std::string& vmId);

    // Restore operations
    bool startRestore(const std::string& vmId, const std::string& backupId);
    bool cancelRestore(const std::string& restoreId);
    bool pauseRestore(const std::string& restoreId);
    bool resumeRestore(const std::string& restoreId);
    RestoreStatus getRestoreStatus(const std::string& restoreId) const;

    // CBT operations
    bool enableCBT(const std::string& vmId);
    bool disableCBT(const std::string& vmId);
    bool isCBTEnabled(const std::string& vmId) const;

    // Backup management
    bool listBackups(std::vector<std::string>& backupIds) override;
    bool deleteBackup(const std::string& backupId) override;
    bool verifyBackup(const std::string& backupId) override;

    // Error handling
    std::string getLastError() const override;
    void clearLastError() override;

    // Progress tracking
    double getProgress() const override;
    void updateProgress(double progress, const std::string& status);
    void setProgressCallback(ProgressCallback callback);
    void setStatusCallback(StatusCallback callback);

private:
    std::shared_ptr<VMwareConnection> connection_;
    double progress_;
    std::string lastError_;
    ProgressCallback progressCallback_;
    StatusCallback statusCallback_;
    std::unordered_map<std::string, std::shared_ptr<BackupJob>> activeOperations_;
    mutable std::mutex mutex_;
    std::string currentSnapshotName_;
    std::string currentVmId_;

    // Helper methods
    bool verifyConnection();
    void cleanupActiveOperations();
    bool initializeVDDK();
    bool saveBackupMetadata(const std::string& backupId, const std::string& vmId,
                           const std::vector<std::string>& diskPaths);
    std::optional<BackupMetadata> getLatestBackupInfo(const std::string& vmId);
    std::string calculateChecksum(const std::string& filePath);
    bool verifyBackupIntegrity(const std::string& backupId);
    bool verifyRestore(const std::string& vmId, const std::string& backupId);

    // CBT operations
    bool initializeCBT(const std::string& vmId);
    bool cleanupCBT(const std::string& vmId);
    bool validateDiskPath(const std::string& diskPath) const;
    bool validateBackupPath(const std::string& backupPath) const;
    bool validateRestorePath(const std::string& restorePath) const;
}; 